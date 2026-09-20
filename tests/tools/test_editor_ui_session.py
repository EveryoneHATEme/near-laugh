"""No-window tests for client evidence, bounded discovery and owned cleanup."""

import copy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_protocol as protocol
import editor_ui_session as helper
from test_editor_ui_protocol import batch_result, observation, result, error as error_result


def session_result(state="closed"):
    return result(state=state, capabilities=[], limits=protocol.LIMITS, file_slots=[], last_request=None, excluded=[])


def uncertain_batch():
    value = batch_result(("passed",))
    value.update(ok=False, failed_index=None, effects="unknown", cleanup="process_terminated", session_state="faulted")
    value["after"] = {**value["after"], "stale": True}
    value["steps"].extend({"index": index, "status": "unknown", "before": None, "after": None} for index in (1, 2))
    value["error"] = error_result(code="disconnected", last_completed_index=0,
        effects="unknown", cleanup="process_terminated", session_state="faulted", snapshot=value["after"])
    return value


def sdk_result(data):
    value = {"structuredContent": data, "content": [{"type": "text", "text": json.dumps(data)}], "isError": not data["ok"]}
    return SimpleNamespace(structured_content=data, content=[SimpleNamespace(text=value["content"][0]["text"])],
                           is_error=not data["ok"], model_dump=lambda **kwargs: value)


class FakeClient:
    def __init__(self, replies):
        self.replies = list(replies)
        self.calls = []
        self.exited = False
        self.protocol_version = protocol.MCP_PROTOCOL_VERSION
        self.server_info = SimpleNamespace(model_dump=lambda **kwargs: {"name": "test-host", "version": "1"})
        self.instructions = "test instructions"

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        self.exited = True

    async def list_tools(self):
        tools = [SimpleNamespace(name=name, input_schema=protocol.INPUT_SCHEMAS[name],
                                 output_schema=protocol.OUTPUT_SCHEMAS[name]) for name in protocol.INPUT_SCHEMAS]
        return SimpleNamespace(tools=tools, model_dump=lambda **kwargs: {"tools": [tool.name for tool in tools]})

    async def call_tool(self, tool, arguments):
        self.calls.append((tool, copy.deepcopy(arguments)))
        if not self.replies:
            raise AssertionError("Unexpected additional SDK call")
        reply = self.replies.pop(0)
        if isinstance(reply, BaseException):
            raise reply
        reply = copy.deepcopy(reply)
        reply["request_id"] = arguments["request_id"]
        return sdk_result(reply)


class SessionHelperTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "run.jsonl"

    async def connected(self, replies):
        session = helper.EditorUiSession("authorized-test", self.path)
        session._open_evidence()
        session.session_id = "session-1"
        session.report["status"] = "running"
        session.client = await session.stack.enter_async_context(FakeClient(replies))
        return session

    def summary(self):
        return json.loads(self.path.with_suffix(".summary.json").read_text(encoding="utf-8"))

    async def test_failure_preserves_steps_evidence_and_closes_once(self):
        failure = batch_result(("passed", "failed", "not_run"))
        session = await self.connected([failure, session_result()])
        error = None
        try:
            await session.execute([{"op": "activate", "target": {"ref": "button"}}] * 3)
        except helper.UiFailure as caught:
            error = caught
        self.assertIsNotNone(error)
        await session.__aexit__(type(error), error, error.__traceback__)
        await session.close()
        summary = self.summary()
        self.assertEqual(summary["status"], "failed")
        self.assertEqual(summary["batches"][0]["steps"], ["passed", "failed", "not_run"])
        self.assertEqual(summary["batches"][0]["observed"]["availability"], "unknown")
        self.assertEqual(summary["cleanup"], "released")
        self.assertEqual([call[0] for call in session.client.calls], ["ui_execute", "ui_session"])
        self.assertTrue(session.client.exited)
        transcript = [json.loads(line) for line in self.path.read_text(encoding="utf-8").splitlines()]
        self.assertEqual(sum(line["direction"] == "response" for line in transcript), 2)

    async def test_lost_mutation_reply_is_unknown_and_never_replayed(self):
        session = await self.connected([TimeoutError("reply lost"), session_result()])
        with self.assertRaises(TimeoutError) as failure:
            await session.execute([{"op": "activate", "target": {"ref": "button"}}] * 2)
        await session.__aexit__(TimeoutError, failure.exception, None)
        summary = self.summary()
        self.assertEqual(summary["status"], "unknown")
        self.assertEqual(summary["batches"][0]["effects"], "unknown")
        self.assertEqual([step["status"] for step in summary["batches"][0]["steps"]], ["unknown", "unknown"])
        self.assertEqual(sum(tool == "ui_execute" for tool, _ in session.client.calls), 1)
        self.assertEqual(summary["tool_calls"], 2)

    async def test_structured_uncertainty_stays_unknown_with_expected_or_unexpected_error(self):
        for expected in (None, "disconnected"):
            with self.subTest(expected_error=expected):
                self.path = Path(self.directory.name) / f"unknown-{expected}.jsonl"
                session = await self.connected([uncertain_batch(), session_result()])
                error = None
                try:
                    await session.execute([{"op": "activate", "target": {"ref": "button"}}] * 3,
                                          expected_error=expected)
                except helper.UiFailure as failure:
                    error = failure
                self.assertEqual(error is None, expected is not None)
                await session.__aexit__(type(error), error, None)
                summary = self.summary()
                self.assertEqual(summary["status"], "unknown")
                self.assertEqual(summary["batches"][0]["status"], "unknown")
                self.assertEqual(summary["batches"][0]["steps"], ["passed", "unknown", "unknown"])
                self.assertEqual(sum(tool == "ui_execute" for tool, _ in session.client.calls), 1)

    async def test_existing_summary_blocks_before_sdk_connection_and_preserves_file(self):
        summary = self.path.with_suffix(".summary.json")
        summary.write_text("existing evidence", encoding="utf-8")
        session = helper.EditorUiSession("authorized-test", self.path)
        with patch.object(helper, "Client") as client:
            with self.assertRaises(FileExistsError):
                async with session:
                    self.fail("Existing evidence must block setup")
            client.assert_not_called()
        self.assertEqual(summary.read_text(encoding="utf-8"), "existing evidence")
        self.assertFalse(self.path.exists())

    async def test_unwritable_summary_blocks_before_sdk_connection(self):
        session = helper.EditorUiSession("authorized-test", self.path)
        with patch.object(Path, "open", side_effect=PermissionError("summary is unwritable")), patch.object(helper, "Client") as client:
            with self.assertRaisesRegex(PermissionError, "summary is unwritable"):
                async with session:
                    self.fail("Unwritable evidence must block setup")
            client.assert_not_called()

    async def test_late_summary_failure_keeps_original_error_and_closes_handles(self):
        session = await self.connected([session_result()])
        handle = session.summary_file
        def failed_write(text):
            raise OSError("disk became full")
        session.summary_file = SimpleNamespace(write=failed_write)
        original = AssertionError("original scenario error")
        with self.assertRaisesRegex(AssertionError, "original scenario error"):
            try:
                raise original
            finally:
                await session.__aexit__(AssertionError, original, None)
        self.assertEqual(session.report["summary_error"], "disk became full")
        self.assertTrue(session.client.exited)
        self.assertTrue(handle.closed)

    async def test_call_budget_reserves_cleanup_and_bounds_report(self):
        session = await self.connected([observation(), observation(), session_result()])
        with patch.object(helper, "MAX_SCENARIO_CALLS", 3):
            await session.observe("properties")
            await session.observe("properties")
            with self.assertRaisesRegex(protocol.ProtocolError, "call budget") as failure:
                await session.execute([{"op": "activate", "target": {"ref": "button"}}])
            await session.__aexit__(protocol.ProtocolError, failure.exception, None)
        self.assertEqual([tool for tool, _ in session.client.calls], ["ui_observe", "ui_observe", "ui_session"])
        self.assertEqual(len(self.summary()["calls"]), 3)

    def test_large_values_link_to_full_evidence_without_a_known_prefix(self):
        value = {"availability": "known", "type": "string", "value": "x" * 8000}
        compact = helper.compact_value(value)
        self.assertEqual(compact["detail"], "retained in transcript")
        self.assertEqual(compact["availability"], "known")
        self.assertNotIn("value", compact)

    async def test_scoped_pagination_preserves_snapshot_and_uses_fresh_ids(self):
        first, second = observation(), observation()
        first["next_cursor"] = "cursor-1"
        second["items"][0]["ref"] = "item-2"
        session = await self.connected([first, second, session_result()])
        observed = await session.observe("properties", key="position.x", page_size=1)
        self.assertEqual([item["ref"] for item in observed["items"]], ["item-1", "item-2"])
        self.assertEqual(session.client.calls[0][1]["filter"], {"key": "position.x"})
        self.assertEqual(session.client.calls[0][1]["depth"], 0)
        self.assertEqual(set(session.client.calls[1][1]), {"protocol_version", "request_id", "session_id", "cursor"})
        await session.__aexit__(None, None, None)
        ids = [arguments["request_id"] for _, arguments in session.client.calls]
        self.assertEqual(ids, ["1", "2", "3"])
        self.assertEqual(self.summary()["status"], "passed")

    async def test_mixed_snapshot_is_not_silently_accepted(self):
        first, second = observation(), observation()
        first["next_cursor"] = "cursor-1"
        second["snapshot"]["snapshot_id"] = "different"
        session = await self.connected([first, second, session_result()])
        with self.assertRaisesRegex(AssertionError, "mixed snapshots") as failure:
            await session.find("properties", key="position.x")
        await session.__aexit__(AssertionError, failure.exception, None)
        self.assertEqual(self.summary()["status"], "failed")

    async def test_ambiguous_find_does_not_choose_first_item(self):
        ambiguous = observation()
        other = copy.deepcopy(ambiguous["items"][0])
        other["ref"] = "item-2"
        ambiguous["items"].append(other)
        ambiguous["coverage"]["snapshot_item_count"] = 2
        session = await self.connected([ambiguous, session_result()])
        with self.assertRaisesRegex(AssertionError, "observed 2") as failure:
            await session.find("properties", key="position.x")
        await session.__aexit__(AssertionError, failure.exception, None)
        self.assertEqual(len(session.client.calls), 2)

    async def test_repeated_cursor_is_bounded_without_retry(self):
        first, second = observation(), observation()
        first["next_cursor"] = second["next_cursor"] = "cursor-1"
        session = await self.connected([first, second, session_result()])
        with self.assertRaisesRegex(AssertionError, "repeated a cursor") as failure:
            await session.observe("properties")
        await session.__aexit__(AssertionError, failure.exception, None)
        self.assertEqual(len(session.client.calls), 3)

    async def test_truncation_does_not_become_unique_target_evidence(self):
        truncated = observation()
        truncated["coverage"].update(truncated=True, limitations=["truncation"])
        session = await self.connected([truncated, session_result()])
        with self.assertRaisesRegex(AssertionError, "Truncated discovery") as failure:
            await session.find("properties", key="position.x")
        await session.__aexit__(AssertionError, failure.exception, None)
        self.assertEqual(self.summary()["status"], "failed")

    async def test_start_reply_loss_closes_sdk_transport_without_guessing_session(self):
        client = FakeClient([TimeoutError("startup reply lost")])
        session = helper.EditorUiSession("authorized-test", self.path)
        with patch.object(helper, "Client", return_value=client), patch.object(helper, "stdio_client", return_value=None):
            with self.assertRaises(TimeoutError):
                async with session:
                    self.fail("Startup should not succeed")
        self.assertTrue(client.exited)
        self.assertEqual(len(client.calls), 1)
        self.assertEqual(client.calls[0][1]["op"], "start")
        self.assertEqual(self.summary()["status"], "unknown")
        self.assertEqual(self.summary()["cleanup"], "unverified")

    async def test_known_preserves_availability_and_expected_failure_can_recover(self):
        for availability in ("unknown", "unavailable", "not_applicable", "truncated"):
            with self.assertRaises(AssertionError):
                helper.known({"availability": availability, "value": "prefix"})
        failure = batch_result(("passed", "failed", "not_run"))
        session = await self.connected([failure, observation(), batch_result(), session_result()])
        await session.execute([{"op": "activate", "target": {"ref": "button"}}] * 3,
                              expected_error="assertion_failed")
        await session.observe("properties", key="position.x")
        await session.execute([{"op": "activate", "target": {"ref": "corrected-button"}}])
        await session.__aexit__(None, None, None)
        self.assertEqual(self.summary()["status"], "passed")
        self.assertEqual([entry["status"] for entry in self.summary()["batches"]], ["failed", "passed"])
        self.assertEqual([args["request_id"] for tool, args in session.client.calls if tool == "ui_execute"], ["1", "3"])


if __name__ == "__main__":
    unittest.main()
