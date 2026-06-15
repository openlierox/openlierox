#!/usr/bin/env python3
"""Tiny static server for the OpenLieroX Wasm build.

Serves build/wasm/output/ on http://localhost:8000 with:
- Correct MIME types for .wasm / .data / .js
- Cross-Origin-Opener-Policy / Cross-Origin-Embedder-Policy headers,
  required by the -pthread build (SharedArrayBuffer).

Optional TLS, for testing on a phone over the LAN: SharedArrayBuffer
needs a *secure context*, which over a LAN IP means HTTPS (localhost is
exempt, a bare LAN IP is not). Set CERTFILE (and optionally KEYFILE) to
serve https:// instead. Example:

    CERTFILE=/tmp/olx-tls/cert.pem KEYFILE=/tmp/olx-tls/key.pem \\
        PORT=8443 python3 serve.py
"""
import http.server
import os
import ssl
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
PORT = int(os.environ.get("PORT", "8000"))
CERTFILE = os.environ.get("CERTFILE")
KEYFILE = os.environ.get("KEYFILE")  # may be None if cert.pem also holds the key


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(http.server.SimpleHTTPRequestHandler.extensions_map)
    extensions_map.update({
        ".wasm": "application/wasm",
        ".data": "application/octet-stream",
        ".js":   "application/javascript",
        ".html": "text/html",
    })

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy",   "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        # Aggressive no-cache so phones don't keep serving a stale build while
        # iterating. no-store alone isn't always honoured by mobile browsers;
        # pair it with no-cache/must-revalidate + the legacy Pragma/Expires.
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


def main():
    os.chdir(ROOT)
    httpd = http.server.ThreadingHTTPServer(("", PORT), Handler)
    scheme = "http"
    if CERTFILE:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=CERTFILE, keyfile=KEYFILE)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
        scheme = "https"
    print(f"Serving {ROOT} on {scheme}://localhost:{PORT}/")
    httpd.serve_forever()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopping.")
        sys.exit(0)
