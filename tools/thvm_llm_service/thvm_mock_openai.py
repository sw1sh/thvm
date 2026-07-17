#!/usr/bin/env python3
"""
Minimal OpenAI-compatible mock server for validating Wolfram Language 15.0
custom LLM backend wiring (stand-in for the real thvm DeepSeek-V2-Lite pod).

Endpoints:
  GET  /v1/models            -> lists one model id ("thvm-dsv2")
  POST /v1/chat/completions  -> returns a deterministic completion whose
                                content is  "THVM-ECHO: <last user message>"
                                so the round-trip is provable.
                                Supports stream=false (default) and stream=true (SSE).

Pure stdlib (http.server) -- no external deps.

Usage:  python3 thvm_mock_openai.py [PORT]   (default 8321)
"""
import json
import sys
import time
import http.server
import socketserver

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8321
MODEL_ID = "thvm-dsv2"


def last_user_message(messages):
    for m in reversed(messages or []):
        if m.get("role") == "user":
            c = m.get("content", "")
            # OpenAI content can be a string or a list of content parts
            if isinstance(c, list):
                c = "".join(
                    part.get("text", "")
                    for part in c
                    if isinstance(part, dict)
                )
            return c
    return ""


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("[mock] " + (fmt % args) + "\n")

    def _send_json(self, obj, code=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.rstrip("/") == "/v1/models":
            self._send_json({
                "object": "list",
                "data": [{
                    "id": MODEL_ID,
                    "object": "model",
                    "created": 1700000000,
                    "owned_by": "thvm",
                }],
            })
        else:
            self._send_json({"error": {"message": "not found"}}, code=404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            req = json.loads(raw or b"{}")
        except json.JSONDecodeError:
            req = {}

        if self.path.rstrip("/") != "/v1/chat/completions":
            self._send_json({"error": {"message": "not found"}}, code=404)
            return

        user = last_user_message(req.get("messages"))
        content = "THVM-ECHO: " + user
        model = req.get("model", MODEL_ID)
        created = int(time.time())
        cid = "chatcmpl-thvmmock0001"

        if req.get("stream"):
            self._stream(content, model, created, cid)
            return

        self._send_json({
            "id": cid,
            "object": "chat.completion",
            "created": created,
            "model": model,
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": content},
                "logprobs": None,
                "finish_reason": "stop",
            }],
            "usage": {
                "prompt_tokens": len(user.split()),
                "completion_tokens": len(content.split()),
                "total_tokens": len(user.split()) + len(content.split()),
            },
        })

    def _stream(self, content, model, created, cid):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        def chunk(delta, finish=None):
            obj = {
                "id": cid, "object": "chat.completion.chunk",
                "created": created, "model": model,
                "choices": [{"index": 0, "delta": delta,
                             "finish_reason": finish}],
            }
            self.wfile.write(b"data: " + json.dumps(obj).encode() + b"\n\n")
            self.wfile.flush()

        chunk({"role": "assistant"})
        for tok in content.split(" "):
            chunk({"content": tok + " "})
        chunk({}, finish="stop")
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


class ThreadingServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


if __name__ == "__main__":
    srv = ThreadingServer(("127.0.0.1", PORT), Handler)
    sys.stderr.write(f"[mock] thvm OpenAI-compatible mock on http://127.0.0.1:{PORT}  model={MODEL_ID}\n")
    srv.serve_forever()
