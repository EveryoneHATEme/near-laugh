"""Test-only transport fault injection around the real owned editor.

No fault switches exist on the production MCP host or its tool interface.
"""

import argparse
from pathlib import Path
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_mcp as host
from editor_ui_process import OwnedProcess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--fault", choices=("protocol", "crash"), required=True)
    args = parser.parse_args()

    class FaultProcess(OwnedProcess):
        def receive(self, timeout=0.05):
            message = super().receive(timeout)
            if args.fault == "protocol" and message and message.get("kind") == "hello":
                message = {**message, "protocol_version": 2}
            return message

        def send(self, message, *, priority=False):
            super().send(message, priority=priority)
            if args.fault == "crash" and message.get("tool") == "ui_execute":
                # Only this Popen-owned child; no process lookup or user editor.
                def kill():
                    if self.child.poll() is None:
                        self.child.kill()
                threading.Timer(0.1, kill).start()

    controller = host.SessionController(environment=args.environment, process_factory=FaultProcess)
    return host.McpServer(controller, sys.stdin.buffer, sys.stdout.buffer,
                          descriptions=host.TOOL_DESCRIPTIONS, format_result=host.tool_result).run()


if __name__ == "__main__":
    raise SystemExit(main())
