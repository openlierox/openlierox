#!/usr/bin/env python3
"""Attach to a *visible* Chrome window via the DevTools Protocol and
stream console + exception events to /tmp/olx-debug.log in real time.

Workflow:
  1. Launches a real Chrome window with --remote-debugging-port=9224.
  2. Connects via CDP, subscribes to Runtime / Console / Log events.
  3. The user drives Chrome (clicks LOCAL PLAY -> START etc.).
  4. Whenever the wasm traps, the full stack trace is appended here
     and to the log file. Exit with Ctrl-C.

Run: /home/ekirprivat/py-venv-global/bin/python build/wasm/attach-debug.py
"""
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import urllib.request
import websocket  # type: ignore

ROOT       = os.path.dirname(os.path.abspath(__file__))
PROFILE    = "/tmp/olx-debug-profile"
LOG_PATH   = "/tmp/olx-debug.log"
DEBUG_PORT = 9224
URL        = "http://localhost:8000/openlierox.html"


def main() -> int:
    shutil.rmtree(PROFILE, ignore_errors=True)
    log = open(LOG_PATH, "w", buffering=1)

    chrome = subprocess.Popen([
        "google-chrome",
        # No --headless: user needs to see the window and click.
        "--user-data-dir=" + PROFILE,
        f"--remote-debugging-port={DEBUG_PORT}",
        "--remote-allow-origins=*",
        "--no-first-run",
        "--no-default-browser-check",
        "--new-window",
        URL,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    ws_url = None
    for _ in range(80):
        time.sleep(0.25)
        try:
            tabs = json.loads(
                urllib.request.urlopen(
                    f"http://127.0.0.1:{DEBUG_PORT}/json", timeout=1
                ).read()
            )
            for tab in tabs:
                if tab.get("type") == "page" and "openlierox.html" in tab.get("url", ""):
                    ws_url = tab["webSocketDebuggerUrl"]
                    break
            if ws_url:
                break
        except Exception:
            pass

    if not ws_url:
        print("ERROR: never connected to Chrome devtools.", file=sys.stderr)
        chrome.terminate()
        return 2

    msg = f"Attached: {ws_url}\nLog: {LOG_PATH}"
    print(msg, flush=True)
    log.write(msg + "\n")
    ws = websocket.create_connection(ws_url, timeout=5)
    msg_id = [0]

    def send(method: str, params: dict | None = None) -> int:
        msg_id[0] += 1
        ws.send(json.dumps({"id": msg_id[0], "method": method,
                            "params": params or {}}))
        return msg_id[0]

    send("Runtime.enable")
    send("Log.enable")
    send("Console.enable")
    send("Page.enable")

    def emit(line: str) -> None:
        print(line, flush=True)
        log.write(line + "\n")

    start = time.time()
    ws.settimeout(0.5)

    interrupted = [False]
    def stop(*_):
        interrupted[0] = True
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    while not interrupted[0] and chrome.poll() is None:
        try:
            raw = ws.recv()
        except websocket.WebSocketTimeoutException:
            continue
        except Exception as e:
            emit(f"ws error: {e}")
            break

        try:
            evt = json.loads(raw)
        except Exception:
            continue

        if "method" not in evt:
            continue

        m = evt["method"]
        p = evt.get("params", {})
        t = time.time() - start

        if m == "Runtime.consoleAPICalled":
            args = p.get("args", [])
            text = " ".join(str(a.get("value", "")) for a in args)
            if text.startswith(("dependency:",
                                "still waiting on run dependencies",
                                "(end of list)")):
                continue
            emit(f"[{t:7.2f}] {p.get('type','log'):>5}: {text}")

        elif m == "Runtime.exceptionThrown":
            ed = p.get("exceptionDetails", {})
            emit(f"[{t:7.2f}] EXCEPTION: {ed.get('text')}")
            ex = ed.get("exception", {}) or {}
            desc = ex.get("description") or ed.get("text") or ""
            for line in desc.splitlines():
                emit(f"           {line}")
            st = ed.get("stackTrace", {}) or {}
            for frame in st.get("callFrames", []) or []:
                fname = frame.get("functionName") or "(anonymous)"
                url = frame.get("url") or ""
                emit(f"           at {fname} ({url}:{frame.get('lineNumber')}:{frame.get('columnNumber')})")

        elif m == "Log.entryAdded":
            e = p.get("entry", {})
            level = e.get("level", "")
            if level in ("warning", "error"):
                emit(f"[{t:7.2f}] {level.upper()}: {e.get('text','')}")
                for frame in (e.get("stackTrace", {}) or {}).get("callFrames", []) or []:
                    fname = frame.get("functionName") or "(anonymous)"
                    url = frame.get("url") or ""
                    emit(f"           at {fname} ({url}:{frame.get('lineNumber')})")

    log.close()
    try:
        ws.close()
    except Exception:
        pass
    print("Detached.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
