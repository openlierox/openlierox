"""Headless shutdown test: a dedicated server must not hang on quit (issue #800).

A dedicated server registers with the master servers over HTTP.
When a master server accepts the connection but never answers,
the registration request stalls inside curl.
On master this blocked OLX's shutdown wait forever
("ThreadPool: waiting for N task(s) ... CHttp ... is still working").

The test routes all of the server's HTTP through a proxy
that points at a black hole (a socket that accepts and never replies),
so a registration request is guaranteed to be in flight and stalled,
then quits the server and checks that it still exits promptly.
"""

import os
import socket
import subprocess
import threading
import time

from harness import CONTROL_DIR, OlxInstance

SHUTDOWN_CONTROL = os.path.join(CONTROL_DIR, "shutdown_control.py")


class BlackHoleProxy:
    """A TCP listener that accepts connections and then never answers.

    Used as OLX's HTTP proxy so every outgoing request connects
    (past the connect timeout) but then stalls waiting for a reply,
    which is exactly the state that used to wedge shutdown.
    """

    def __init__(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(16)
        self.port = self._sock.getsockname()[1]
        self.connections = 0
        self._held = []
        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        self._sock.settimeout(0.5)
        while not self._stop:
            try:
                conn, _ = self._sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            # Hold the connection open and stay silent.
            self.connections += 1
            self._held.append(conn)

    def stop(self):
        self._stop = True
        for conn in self._held:
            conn.close()
        self._sock.close()


def test_dedicated_server_shutdown_not_blocked_by_registration(olx_binary, tmp_path):
    """Quitting a registering dedicated server exits promptly (issue #800).

    Reproduces the hang: with a stalled registration request in flight,
    master blocks in the thread-pool wait indefinitely, so this fails
    (the process never exits) until the request is interruptible.
    """
    proxy = BlackHoleProxy()
    server = OlxInstance(
        olx_binary, "server", str(tmp_path / "server"), SHUTDOWN_CONTROL,
        env={
            "OLX_PORT": "23400",
            "OLX_HTTP_PROXY": "127.0.0.1:%d" % proxy.port,
            "OLX_REGISTER_WAIT": "3",
        },
    ).start()
    try:
        assert server.wait_for("SERVER_QUIT", timeout=40), (
            "server never reached the quit point:\n" + server.read_log()
        )

        # The registration request really went out to the (stalled) proxy,
        # so the shutdown-blocking condition genuinely existed.
        assert proxy.connections >= 1, (
            "no HTTP request reached the proxy; the hang was not exercised"
        )

        # After quit, OLX must exit despite the hung request still in flight.
        start = time.time()
        try:
            server.proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            assert False, (
                "OLX hung on shutdown (>20s) with a stalled registration "
                "request in flight:\n" + server.read_log()
            )
        shutdown_time = time.time() - start
        assert shutdown_time < 15, (
            "shutdown took %.1fs; the stalled registration request was not "
            "interrupted promptly" % shutdown_time
        )
    finally:
        server.stop()
        proxy.stop()
