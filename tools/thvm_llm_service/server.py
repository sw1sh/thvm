#!/usr/bin/env python3
"""
OpenAI-compatible front-end for the thvm DeepSeek-V2-Lite serve loop
(tools/dsv2_decode_bench.c DSV2_SERVE=1).  Spawns the C binary as a warm
child (weights + captured step stay resident), applies the DeepSeek-V2 chat
template via transformers AutoTokenizer, and speaks the exact JSON schema of
tools/thvm_llm_service/thvm_mock_openai.py, so a Wolfram LLM service
registered by register_thvm.wls works unchanged against the real model.

Endpoints:
  GET  /v1/models            -> lists one model id ("thvm-dsv2")
  POST /v1/chat/completions  -> greedy generation on the pod GPU.
                                temperature is accepted; >0 still returns the
                                greedy result plus a "note" field (thvm decode
                                is temperature-0 by construction).
                                max_tokens -> per-request "|max_new" cap.
                                stop sequences truncate the decoded text
                                (the C loop already stops at the eos id).
                                stream=true replays the mock's SSE shape.

Child protocol (one line each way, see serve_loop in dsv2_decode_bench.c):
  -> "id,id,...|max_new\n"      <- "ids: id,id,...\n"

Dependencies: stdlib + transformers (tokenizer only; no torch import needed
for tokenization).

Usage:
  python3 server.py --bin ~/thvm/bin/dsv2_decode_bench \
      --weights ~/.cache/thvm/dsv2.pack --snap <hf snapshot dir> \
      [--port 8399] [--host 0.0.0.0] [--spec 12] [--dev cuda]
"""
import argparse
import json
import os
import subprocess
import sys
import threading
import time
import http.server
import socketserver

MODEL_ID = "thvm-dsv2"


def log(msg):
    sys.stderr.write(f"[thvm-serve] {msg}\n")
    sys.stderr.flush()


class ThvmChild:
    """The warm C serve process; one request line in, one ids line out."""

    def __init__(self, args):
        env = dict(os.environ)
        env.update({"DEV": args.dev, "DSV2_WEIGHTS": os.path.expanduser(args.weights),
                    "DSV2_SERVE": "1"})
        if args.spec > 0:
            env["DSV2_SPEC"] = str(args.spec)
        self.proc = subprocess.Popen(
            [os.path.expanduser(args.bin)], env=env, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.lock = threading.Lock()
        threading.Thread(target=self._pump_stderr, daemon=True).start()
        t0 = time.time()
        for line in self.proc.stdout:
            log(f"child: {line.rstrip()}")
            if line.strip() == "ready":
                log(f"child ready in {time.time() - t0:.1f} s")
                return
        raise RuntimeError("child exited before ready; see its stderr above")

    def _pump_stderr(self):
        for line in self.proc.stderr:
            sys.stderr.write("[child] " + line)
            sys.stderr.flush()

    def generate(self, ids, max_new):
        """Prompt token ids -> generated token ids (greedy, eos-truncated)."""
        with self.lock:
            if self.proc.poll() is not None:
                raise RuntimeError(f"child died (exit {self.proc.returncode})")
            self.proc.stdin.write(",".join(map(str, ids)) + f"|{max_new}\n")
            self.proc.stdin.flush()
            line = self.proc.stdout.readline()
        if not line.startswith("ids: "):
            raise RuntimeError(f"bad child response: {line!r}")
        body = line[len("ids: "):].strip()
        return [int(t) for t in body.split(",")] if body else []


def to_template_messages(messages):
    """OpenAI messages -> chat-template messages (content parts flattened)."""
    out = []
    for m in messages or []:
        c = m.get("content", "")
        if isinstance(c, list):
            c = "".join(p.get("text", "") for p in c if isinstance(p, dict))
        out.append({"role": m.get("role", "user"), "content": c})
    return out


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    child = None
    tokenizer = None
    default_max = 256

    def log_message(self, fmt, *args):
        log(fmt % args)

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
        try:
            self._chat(req)
        except Exception as e:  # noqa: BLE001 - surface any failure as JSON
            log(f"ERROR: {e}")
            self._send_json({"error": {"message": str(e)}}, code=500)

    def _chat(self, req):
        msgs = to_template_messages(req.get("messages"))
        ids = self.tokenizer.apply_chat_template(msgs, add_generation_prompt=True)
        if not isinstance(ids, list):        # transformers 5.x: BatchEncoding
            ids = ids["input_ids"]
        max_new = int(req.get("max_tokens") or req.get("max_completion_tokens")
                      or self.default_max)
        t0 = time.time()
        out_ids = self.child.generate(ids, max_new)
        dt = time.time() - t0
        # the C loop already stops BEFORE the eos id; drop one defensively
        eos = self.tokenizer.eos_token_id
        if eos in out_ids:
            out_ids = out_ids[:out_ids.index(eos)]
        content = self.tokenizer.decode(out_ids, skip_special_tokens=True)
        finish = "length" if len(out_ids) >= max_new else "stop"
        stop = req.get("stop")
        for s in [stop] if isinstance(stop, str) else (stop or []):
            if s and s in content:
                content = content[:content.index(s)]
                finish = "stop"
        n = max(len(out_ids), 1)
        log(f"gen: prompt={len(ids)} tok, out={len(out_ids)} tok in {dt * 1e3:.0f} ms"
            f"  ({dt * 1e3 / n:.1f} ms/tok, {len(out_ids) / dt:.1f} tok/s e2e)")

        model = req.get("model", MODEL_ID)
        created = int(time.time())
        cid = f"chatcmpl-thvm{created:x}"
        resp = {
            "id": cid,
            "object": "chat.completion",
            "created": created,
            "model": model,
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": content},
                "logprobs": None,
                "finish_reason": finish,
            }],
            "usage": {
                "prompt_tokens": len(ids),
                "completion_tokens": len(out_ids),
                "total_tokens": len(ids) + len(out_ids),
            },
        }
        temp = req.get("temperature")
        if temp is not None and float(temp) > 0:
            resp["note"] = "thvm is greedy (temperature 0)"
        if req.get("stream"):
            self._stream(content, model, created, cid)
            return
        self._send_json(resp)

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
        chunk({"content": content})
        chunk({}, finish="stop")
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


class ThreadingServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, help="dsv2_decode_bench binary")
    ap.add_argument("--weights", required=True, help="DSV2_WEIGHTS pack")
    ap.add_argument("--snap", required=True, help="HF snapshot dir (tokenizer)")
    ap.add_argument("--port", type=int, default=8399)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--dev", default="cuda")
    ap.add_argument("--spec", type=int, default=int(os.environ.get("DSV2_SPEC", 0)))
    ap.add_argument("--max-new", type=int, default=256, help="default max_tokens")
    args = ap.parse_args()

    from transformers import AutoTokenizer
    Handler.tokenizer = AutoTokenizer.from_pretrained(os.path.expanduser(args.snap))
    log(f"tokenizer loaded (eos={Handler.tokenizer.eos_token_id})")
    Handler.child = ThvmChild(args)
    Handler.default_max = args.max_new
    srv = ThreadingServer((args.host, args.port), Handler)
    log(f"thvm OpenAI server on http://{args.host}:{args.port}  model={MODEL_ID}"
        f"  spec={args.spec}")
    srv.serve_forever()


if __name__ == "__main__":
    main()
