"""Official MCP SDK adapter; no editor semantics or MCP method implementation.

The byte transport enforces the project's bounds before handing messages to
the SDK. The SDK owns initialization, dispatch, errors and cancellation.
"""

from concurrent.futures import ThreadPoolExecutor
from functools import partial
import queue
import sys
import threading
import time

import anyio
from mcp.server import Server, ServerRequestContext
from mcp.shared.exceptions import MCPError
from mcp.shared.message import SessionMessage
from mcp.types import (CallToolRequestParams, CallToolResult, ListToolsResult,
                       PaginatedRequestParams, Tool, ToolAnnotations,
                       jsonrpc_message_adapter)

import editor_ui_protocol as protocol
from editor_ui_process import LineReader, LineWriter

SERVER_INSTRUCTIONS = (
    "Use ui_session to start an authorized disposable fixture and always close it. "
    "Discover targets/commit methods with scoped ui_observe; send known actions and assertions "
    "together in ui_execute (up to 64 steps). Use app_inspect selected fields to verify applied "
    "state independently: UI draft/input is not application. No Windows mouse/keyboard fallback. "
    "Build affected targets outside MCP before testing. Follow pagination/availability and ref "
    "lifetimes; resolve targets anew per session. Failure stops the suffix, retaining prior effects. "
    "Observe after verified cleanup before a corrected request; never automatically replay mutations "
    "after timeout/disconnect/lost responses. Report blocked/unsupported and unknown honestly. "
    "Functional success does not establish visual acceptance."
)


class McpServer:
    def __init__(self, controller, input_stream, output_stream, *, descriptions, format_result):
        self.controller = controller
        self.reader = LineReader(input_stream)
        self.writer = LineWriter(output_stream)
        self.format_result = format_result
        self.executor = ThreadPoolExecutor(max_workers=4, thread_name_prefix="editor-ui-call")
        self.controls = ThreadPoolExecutor(max_workers=2, thread_name_prefix="editor-ui-control")
        self.active = {False: 0, True: 0}
        self.connection_ending = False
        self.call_cancellations = set()
        self.tools = [Tool(name=name, description=descriptions[name],
                           input_schema=schema, output_schema=protocol.OUTPUT_SCHEMAS[name],
                           annotations=ToolAnnotations(read_only_hint=name in ("ui_observe", "app_inspect"),
                                                       open_world_hint=False))
                      for name, schema in protocol.INPUT_SCHEMAS.items()]
        self.server = Server("near-laugh-editor-ui", version="1",
                             instructions=SERVER_INSTRUCTIONS,
                             on_list_tools=self.list_tools, on_call_tool=self.call_tool)

    async def list_tools(self, ctx: ServerRequestContext,
                         params: PaginatedRequestParams | None) -> ListToolsResult:
        if params and params.cursor is not None:
            raise MCPError(-32602, "Tools list has no continuation")
        return ListToolsResult(tools=self.tools)

    async def call_tool(self, ctx: ServerRequestContext,
                        params: CallToolRequestParams) -> CallToolResult:
        tool, arguments = params.name, params.arguments or {}
        try:
            protocol.validate_request(tool, arguments)
        except protocol.ProtocolError as error:
            raise MCPError(-32602, error.message) from error
        if tool == "ui_session" and arguments["op"] == "status":
            value = self.controller.call(tool, arguments)
        else:
            control = tool == "ui_session" and arguments["op"] in ("cancel", "close")
            if self.active[control] >= (2 if control else 14):
                value = self.controller.failure(tool, arguments, "busy", "Host request queue is full")
            else:
                self.active[control] += 1
                cancelled = threading.Event()
                self.call_cancellations.add(cancelled)
                future = (self.controls if control else self.executor).submit(
                    self.controller.call, tool, arguments, cancelled=cancelled)
                try:
                    while not future.done():
                        await anyio.sleep(0.005)
                    value = future.result()
                except anyio.get_cancelled_exc_class():
                    # A cancelled queued call must never start later. If already
                    # running, route cancellation to the owning editor, and keep
                    # this slot until its result/cleanup or bounded termination.
                    cancelled.set()
                    if not future.cancel() and not self.connection_ending:
                        with anyio.CancelScope(shield=True):
                            deadline = time.monotonic() + protocol.LIMITS["cleanup_ms"] / 1000
                            disconnect = partial(self.controller.disconnect, deadline=deadline)
                            if tool == "ui_session" and arguments["op"] == "start":
                                with self.controller.lock:
                                    owns_start = self.controller.start_request is arguments
                                if owns_start:
                                    await anyio.to_thread.run_sync(disconnect)
                            with anyio.move_on_after(max(0, deadline - time.monotonic() - .2)):
                                sent = False
                                while not future.done():
                                    if tool == "ui_execute" and not sent:
                                        with self.controller.lock:
                                            running = self.controller.running
                                            if running is not None and running["request_id"] == arguments["request_id"]:
                                                self.controller.cancel_execution(arguments["request_id"])
                                                sent = True
                                    await anyio.sleep(0.005)
                            if not future.done():
                                await anyio.to_thread.run_sync(disconnect)
                    raise
                finally:
                    self.call_cancellations.discard(cancelled)
                    self.active[control] -= 1
        return CallToolResult.model_validate(self.format_result(self.controller, tool, arguments, value))

    async def _run(self):
        incoming, read_stream = anyio.create_memory_object_stream(0)
        write_stream, outgoing = anyio.create_memory_object_stream(0)
        exit_code = 0
        disconnected = False

        async def disconnect():
            nonlocal disconnected
            if disconnected:
                return
            disconnected = self.connection_ending = True
            # A running worker can still be before _start's ownership lock.
            # Mark it before closing a currently empty session, so it cannot
            # acquire an editor after EOF cleanup has already returned.
            for cancelled in self.call_cancellations:
                cancelled.set()
            self.executor.shutdown(wait=False, cancel_futures=True)
            self.controls.shutdown(wait=False, cancel_futures=True)
            stop = (partial(self.controller.disconnect, "invalid_request", "Controller protocol failed")
                    if exit_code else self.controller.disconnect)
            with anyio.CancelScope(shield=True):
                await anyio.to_thread.run_sync(stop)

        async def receive():
            nonlocal exit_code
            async with incoming:
                while True:
                    fault = self.reader.poll_fault() or self.writer.poll_fault()
                    if fault:
                        # Framing is a transport failure, not a second JSON-RPC
                        # implementation. Closing cancels the SDK request scopes.
                        print(f"editor-ui transport: {fault.code}: {fault.message}", file=sys.stderr)
                        exit_code = 1
                        await disconnect()
                        return
                    try:
                        message = self.reader.messages.get_nowait()
                    except queue.Empty:
                        if self.reader.eof:
                            await disconnect()
                            return
                        await anyio.sleep(0.005)
                        continue
                    try:
                        parsed = jsonrpc_message_adapter.validate_python(message, by_name=False)
                    except ValueError as error:
                        await incoming.send(error)
                    else:
                        await incoming.send(SessionMessage(parsed))

        async def send():
            async with outgoing:
                async for message in outgoing:
                    self.writer.send(message.message.model_dump(by_alias=True, exclude_unset=True))

        async def watchdog():
            while True:
                # Tick can reap a failed child: keep it off the SDK event loop.
                await anyio.to_thread.run_sync(self.controller.tick)
                await anyio.sleep(0.01)

        try:
            async with anyio.create_task_group() as group:
                group.start_soon(receive)
                group.start_soon(send)
                group.start_soon(watchdog)
                await self.server.run(read_stream, write_stream, self.server.create_initialization_options())
                group.cancel_scope.cancel()
        except Exception as error:
            print(f"editor-ui SDK transport failed: {str(error)[:512]}", file=sys.stderr)
            exit_code = 1
        finally:
            with anyio.CancelScope(shield=True):
                await disconnect()
                await anyio.to_thread.run_sync(self.writer.finish)
        return exit_code

    def run(self):
        return anyio.run(self._run)
