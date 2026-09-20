"""Official SDK stdio client for arbitrary runtime JSON-lines tool requests.

Input: {"tool": <name>, "arguments": <typed request>}. Output IDs correlate
input lines locally; MCP wire IDs and initialization belong to the SDK.
Transcripts retain the negotiated connection facts and actual tool payloads.
"""

import argparse
from functools import partial
from pathlib import Path
import queue
import sys

import anyio
from mcp import Client
from mcp.client.stdio import StdioServerParameters, stdio_client
from mcp.shared.exceptions import MCPError

import editor_ui_protocol as protocol
from editor_ui_process import LineReader, LineWriter
from editor_ui_fixtures import load_fixtures


def host_parameters(environment=None):
    """Fixed repository host; no caller-supplied executable or shell."""
    args = ["-B", str(Path(__file__).resolve().with_name("editor_ui_mcp.py"))]
    if environment:
        args += ["--environment", environment]
    return StdioServerParameters(command=sys.executable, args=args)


class Transcript:
    def __init__(self, path):
        self.file = Path(path).open("xb", buffering=0)
        self.size = 0

    def record(self, direction, message):
        data = protocol.encode_message({"direction": direction, "message": message})
        if self.size + len(data) > 64 * 1024 * 1024:
            raise protocol.ProtocolError("limit_exceeded", "Transcript reached its 64 MiB bound")
        self.file.write(data)
        self.size += len(data)

    def close(self):
        self.file.close()


async def run(args):
    transcript = Transcript(args.transcript)
    incoming = LineReader(sys.stdin.buffer)
    output = LineWriter(sys.stdout.buffer)
    active = 0
    sequence = 0
    had_error = False
    try:
        # Legacy mode deliberately exercises initialize/initialized. The SDK
        # also supports modern discovery; no protocol is reimplemented here.
        async with Client(stdio_client(host_parameters(args.environment)), mode="legacy",
                          read_timeout_seconds=70) as client:
            transcript.record("connection", {"method": "initialize", "protocolVersion": client.protocol_version,
                              "serverInfo": client.server_info.model_dump(by_alias=True, exclude_none=True)})

            async def call(index, tool, arguments):
                nonlocal active, had_error
                try:
                    transcript.record("request", {"id": index, "method": "tools/call",
                                                  "params": {"name": tool, "arguments": arguments}})
                    result = await client.call_tool(tool, arguments)
                    value = result.model_dump(by_alias=True, exclude_none=True)
                    if result.structured_content is not None:
                        protocol.validate_result(tool, result.structured_content, request=arguments)
                    response = {"id": index, "result": value}
                    had_error |= result.is_error
                except MCPError as error:
                    response = {"id": index, "error": error.error.model_dump(by_alias=True, exclude_none=True)}
                    had_error = True
                finally:
                    active -= 1
                transcript.record("response", response)
                output.send(response)

            async with anyio.create_task_group() as group:
                if args.start:
                    sequence += 1
                    active += 1
                    await call(sequence, "ui_session", {"protocol_version": 1, "request_id": "client-start",
                               "op": "start", "fixture": args.start, "environment": args.environment})
                while True:
                    fault = incoming.poll_fault() or output.poll_fault()
                    if fault:
                        raise fault
                    if incoming.eof and incoming.messages.empty() and not active:
                        break
                    if active < 16:
                        try:
                            request = incoming.messages.get_nowait()
                        except queue.Empty:
                            pass
                        else:
                            if set(request) != {"tool", "arguments"} or type(request["tool"]) is not str:
                                raise protocol.ProtocolError("invalid_request", "Expected {tool, arguments}")
                            protocol.validate_request(request["tool"], request["arguments"])
                            sequence += 1
                            active += 1
                            group.start_soon(call, sequence, request["tool"], request["arguments"])
                    await anyio.sleep(0.005)
        return int(had_error)
    finally:
        transcript.close()
        if not await anyio.to_thread.run_sync(output.finish):
            raise protocol.ProtocolError("timeout", "Client output pipe did not drain")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", help="Operator-authorized test desktop/profile for this run")
    parser.add_argument("--start", choices=tuple(load_fixtures()))
    parser.add_argument("--transcript", type=Path, required=True, help="New transcript, never overwritten")
    args = parser.parse_args(argv)
    if args.start and not args.environment:
        parser.error("--start requires the operator's --environment")
    try:
        return anyio.run(partial(run, args))
    except Exception as error:
        print(str(error)[:512], file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
