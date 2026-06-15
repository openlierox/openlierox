#!/usr/bin/env python3
"""Drive the Wasm build under headless Chrome via the Chrome DevTools
Protocol. Tails console + page errors in real time and saves periodic
screenshots, so we can see exactly where the runtime gets stuck.

Usage: run-headless.py [seconds=120] [screenshot_every=5]
"""
import base64
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
PROFILE    = "/tmp/olx-cdp-profile"
SHOTS_DIR  = "/tmp/olx-shots"
DEBUG_PORT = 9223
URL        = "http://localhost:8000/openlierox.html"

DURATION   = int(sys.argv[1]) if len(sys.argv) > 1 else 120
SHOT_EVERY = int(sys.argv[2]) if len(sys.argv) > 2 else 5

# Optional: list of "TIMESTAMP CLICK x y" or "TIMESTAMP KEY name" actions.
# Parsed from sys.argv[3:] as triples ("at:CLICK:300,400", "at:25:KEY:Down", ...).
SCRIPT = []
for tok in sys.argv[3:]:
    parts = tok.split(":")
    if len(parts) >= 3 and parts[0] == "at":
        SCRIPT.append((float(parts[1]), parts[2], ":".join(parts[3:])))


def main() -> int:
    shutil.rmtree(PROFILE,   ignore_errors=True)
    shutil.rmtree(SHOTS_DIR, ignore_errors=True)
    os.makedirs(SHOTS_DIR, exist_ok=True)

    chrome = subprocess.Popen([
        "google-chrome",
        "--headless=new",
        # Keep GPU on — disabling it falls back to a software path
        # that hides issues real Chrome users hit (e.g. WebGL-related
        # wasm traps). Use ANGLE+SwiftShader so we don't need a GPU.
        "--use-gl=angle",
        "--use-angle=swiftshader",
        "--no-sandbox",
        "--disable-dev-shm-usage",
        "--user-data-dir=" + PROFILE,
        f"--remote-debugging-port={DEBUG_PORT}",
        "--remote-allow-origins=*",
        "--window-size=1024,768",
        URL,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    ws_url = None
    for _ in range(40):
        time.sleep(0.25)
        try:
            tabs = json.loads(
                urllib.request.urlopen(
                    f"http://127.0.0.1:{DEBUG_PORT}/json", timeout=1
                ).read()
            )
            for tab in tabs:
                if tab.get("type") == "page" and tab.get("url"):
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

    print(f"Connected to {ws_url}", flush=True)
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

    def cdp_call(method: str, params: dict) -> None:
        """Send a CDP request and drain pending events until the reply
        matching this id arrives. Used for synchronous calls like
        screenshots / input dispatch."""
        msg_id[0] += 1
        my = msg_id[0]
        ws.send(json.dumps({"id": my, "method": method, "params": params}))
        prev_to = ws.gettimeout()
        ws.settimeout(5)
        t0 = time.time()
        result = None
        try:
            while time.time() - t0 < 5:
                try:
                    r = ws.recv()
                except websocket.WebSocketTimeoutException:
                    break
                obj = json.loads(r)
                if obj.get("id") == my:
                    result = obj.get("result")
                    break
        finally:
            ws.settimeout(prev_to)
        return result

    def take_shot(label: str) -> None:
        r = cdp_call("Page.captureScreenshot", {"format": "png"})
        if r and r.get("data"):
            path = os.path.join(SHOTS_DIR, label)
            with open(path, "wb") as f:
                f.write(base64.b64decode(r["data"]))
            print(f"           saved {path}", flush=True)

    def canvas_rect():
        """Return the canvas DOM bounding rect (left, top, width, height)
        in CSS pixels — we'll add a viewport offset of (left, top) to
        convert canvas-internal click coords to absolute mouse coords."""
        r = cdp_call("Runtime.evaluate", {
            "expression": "JSON.stringify((() => { "
                          "const c = document.getElementById('canvas');"
                          "const r = c.getBoundingClientRect();"
                          "return { l: r.left, t: r.top, w: r.width, "
                          "         h: r.height, iw: c.width, ih: c.height };"
                          "})())",
            "returnByValue": True})
        if not r:
            return None
        try:
            return json.loads(r["result"]["value"])
        except Exception:
            return None

    def click(x: int, y: int) -> None:
        """x/y are canvas-internal (i.e. CSS-pixel) coords. SDL's
        SDL_RenderSetLogicalSize then scales these to OLX-logical
        (640x480) under the hood."""
        rect = canvas_rect()
        if rect:
            sx = rect["l"] + (x / rect["iw"]) * rect["w"]
            sy = rect["t"] + (y / rect["ih"]) * rect["h"]
            print(f"           canvas_rect={rect}", flush=True)
        else:
            sx, sy = x, y
        sx, sy = int(sx), int(sy)
        # Move first so any hover-tracking widgets see us inside.
        cdp_call("Input.dispatchMouseEvent",
                 {"type": "mouseMoved", "x": sx, "y": sy, "button": "left"})
        time.sleep(0.05)
        # Use synthesizeTapGesture instead of dispatchMouseEvent —
        # the latter generates a synthetic JS MouseEvent that some SDL
        # builds don't treat as a real user click; the former goes
        # through the platform layer and looks like a real tap to the
        # browser, which OLX's CButton handler then sees as a proper
        # press+release with the gesture duration as hold time.
        cdp_call("Input.synthesizeTapGesture",
                 {"x": sx, "y": sy, "duration": 120,
                  "tapCount": 1, "gestureSourceType": "mouse"})
        print(f"           click(canvas={x},{y} -> viewport={sx},{sy})", flush=True)

    def press_key(name: str) -> None:
        cdp_call("Input.dispatchKeyEvent",
                 {"type": "keyDown", "key": name, "code": name})
        cdp_call("Input.dispatchKeyEvent",
                 {"type": "keyUp",   "key": name, "code": name})
        print(f"           key({name})", flush=True)

    start = time.time()
    last_shot = 0.0
    deadline = start + DURATION
    pending_actions = list(SCRIPT)
    ws.settimeout(0.25)

    while time.time() < deadline:
        try:
            raw = ws.recv()
        except websocket.WebSocketTimeoutException:
            raw = None

        if raw:
            try:
                evt = json.loads(raw)
            except Exception:
                evt = None

            if evt and "method" in evt:
                m = evt["method"]
                p = evt.get("params", {})

                if m == "Runtime.consoleAPICalled":
                    args = p.get("args", [])
                    text = " ".join(str(a.get("value", "")) for a in args)
                    if not text.startswith(("dependency:",
                                            "still waiting on run dependencies",
                                            "(end of list)")):
                        print(f"[{time.time()-start:6.1f}] {p.get('type','log'):>5}: {text}",
                              flush=True)

                elif m == "Runtime.exceptionThrown":
                    ed = p.get("exceptionDetails", {})
                    print(f"[{time.time()-start:6.1f}] EXC: {ed.get('text')} @ {ed.get('url')}:{ed.get('lineNumber')}",
                          flush=True)
                    exception = ed.get("exception", {})
                    if exception.get("description"):
                        for line in exception["description"].splitlines()[:20]:
                            print(f"            {line}", flush=True)

                elif m == "Log.entryAdded":
                    e = p.get("entry", {})
                    if e.get("level") in ("warning", "error"):
                        print(f"[{time.time()-start:6.1f}] {e['level'].upper()}: {e.get('text','')}",
                              flush=True)

        now = time.time() - start

        # Fire any scripted actions whose time has come.
        while pending_actions and pending_actions[0][0] <= now:
            _, action, arg = pending_actions.pop(0)
            print(f"[{now:6.1f}] action: {action} {arg}", flush=True)
            try:
                if action == "CLICK":
                    x, y = (int(v) for v in arg.split(","))
                    click(x, y)
                elif action == "KEY":
                    press_key(arg)
                elif action == "SHOT":
                    take_shot(arg)
            except Exception as e:
                print(f"action error: {e}", flush=True)

        # Periodic screenshot
        if now - last_shot >= SHOT_EVERY:
            last_shot = now
            try:
                take_shot(f"shot-{int(now):04d}s.png")
            except Exception as e:
                print(f"screenshot error: {e}", flush=True)

    print("--- DONE ---", flush=True)
    ws.close()
    chrome.send_signal(signal.SIGTERM)
    try:
        chrome.wait(timeout=5)
    except subprocess.TimeoutExpired:
        chrome.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
