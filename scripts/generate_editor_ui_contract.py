"""Generate the private C++ contract from the same schemas advertised by MCP."""
import json
from pathlib import Path
import sys

from editor_ui_protocol import HANDSHAKE_SCHEMA, INPUT_SCHEMAS, OUTPUT_SCHEMAS, make_handshake

output = Path(sys.argv[1])
fingerprint = sys.argv[2]
contract = {"inputs": INPUT_SCHEMAS, "outputs": OUTPUT_SCHEMAS,
            "handshake_schema": HANDSHAKE_SCHEMA, "hello": make_handshake(fingerprint)}
encoded = json.dumps(contract, ensure_ascii=True, separators=(",", ":"))
output.write_text("\n".join('R"NLUI(' + encoded[index:index + 16384] + ')NLUI",'
                           for index in range(0, len(encoded), 16384)) + "\n",
                  encoding="utf-8")
