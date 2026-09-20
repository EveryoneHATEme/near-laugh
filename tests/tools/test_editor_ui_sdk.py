"""Official SDK client over actual stdio. These checks open no editor window."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

from jsonschema import Draft202012Validator
from mcp import Client
from mcp.client.stdio import stdio_client
from mcp.shared.exceptions import MCPError

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_protocol as protocol
from editor_ui_client import host_parameters


class SdkStdioTests(unittest.IsolatedAsyncioTestCase):
    async def test_initialization_schemas_calls_and_recovery(self):
        with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as errors:
            async with Client(stdio_client(host_parameters(), errlog=errors), mode="legacy",
                              read_timeout_seconds=10) as client:
                self.assertEqual(client.protocol_version, protocol.MCP_PROTOCOL_VERSION)
                self.assertEqual(client.server_info.name, "near-laugh-editor-ui")
                tools = (await client.list_tools()).tools
                self.assertEqual({tool.name for tool in tools}, set(protocol.INPUT_SCHEMAS))
                for tool in tools:
                    Draft202012Validator.check_schema(tool.input_schema)
                    Draft202012Validator.check_schema(tool.output_schema)
                    self.assertEqual(tool.input_schema, protocol.INPUT_SCHEMAS[tool.name])
                    self.assertEqual(tool.output_schema, protocol.OUTPUT_SCHEMAS[tool.name])
                status = {"protocol_version": 1, "request_id": "sdk-status", "op": "status"}
                result = await client.call_tool("ui_session", status)
                self.assertFalse(result.is_error)
                self.assertEqual(result.structured_content["state"], "closed")
                self.assertEqual(json.loads(result.content[0].text), result.structured_content)
                Draft202012Validator(protocol.OUTPUT_SCHEMAS["ui_session"]).validate(result.structured_content)
                # Wrong types, unknown fields/actions, excessive batches and
                # arbitrary executable parameters are rejected before execution.
                execute = {"protocol_version": 1, "request_id": "1", "session_id": "none"}
                invalid = [
                    ("ui_session", {**status, "unexpected": True}),
                    ("ui_session", {**status, "protocol_version": "1"}),
                    ("ui_session", {**status, "executable": "anything.exe"}),
                    ("ui_execute", {**execute, "steps": [{"op": "shell"}]}),
                    ("ui_execute", {**execute, "steps": [{"op": "activate", "target": {"ref": "x"}}] * 65}),
                    ("app_inspect", {**execute, "projection": "arbitrary.member"}),
                ]
                for name, args in invalid:
                    with self.subTest(name=name, arguments=args), self.assertRaises(MCPError) as failure:
                        await client.call_tool(name, args)
                    self.assertEqual(failure.exception.code, -32602)
                denied = await client.call_tool("ui_session", {"protocol_version": 1, "request_id": "sdk-start",
                    "op": "start", "fixture": "apartment-stairs", "environment": "unpermitted"})
                self.assertTrue(denied.is_error)
                self.assertEqual(denied.structured_content["error"]["code"], "environment_not_authorized")
                self.assertEqual((await client.call_tool("ui_session", status)).structured_content["state"], "closed")

    async def test_sdk_modern_negotiation_also_uses_same_four_tools(self):
        with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as errors:
            async with Client(stdio_client(host_parameters(), errlog=errors), read_timeout_seconds=10) as client:
                self.assertEqual({tool.name for tool in (await client.list_tools()).tools}, set(protocol.INPUT_SCHEMAS))
                result = await client.call_tool("ui_session", {"protocol_version": 1, "request_id": "status", "op": "status"})
                self.assertEqual(result.structured_content["state"], "closed")


if __name__ == "__main__":
    unittest.main()
