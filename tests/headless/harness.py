"""Helpers for driving OpenLieroX headlessly from the test suite.

Every OLX instance runs as a dedicated process (``-dedicated``),
which needs no display or audio,
so the whole suite runs on a bare CI runner with no X server.
Each instance runs a Python 3 control script from ``control/`` via ``-script``;
the script drives the instance
and prints progress markers to stderr, which the harness waits on.

OLX owns the only control channel (the script pipe),
so rather than driving instances live,
each control script is self-contained
and the harness coordinates by waiting for those markers
in each instance's combined output log.
"""

import os
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CONTROL_DIR = os.path.join(HERE, "control")
GAMEDIR = os.path.join(REPO_ROOT, "share", "gamedir")

SERVER_CONTROL = os.path.join(CONTROL_DIR, "server_control.py")
CLIENT_CONTROL = os.path.join(CONTROL_DIR, "client_control.py")


def find_binary():
    """Return the path to the openlierox binary, or None if not built.

    Honours ``OLX_BINARY`` if set;
    otherwise looks in the usual build directories.
    """
    env = os.environ.get("OLX_BINARY")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return env
    candidates = [
        os.path.join(REPO_ROOT, "build-output", "bin", "openlierox"),
        os.path.join(REPO_ROOT, "build", "bin", "openlierox"),
        os.path.join(REPO_ROOT, "bin", "openlierox"),
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None


class OlxInstance:
    """A single headless OLX process and its combined output log."""

    def __init__(self, binary, name, home, script, connect=None, env=None):
        self.binary = binary
        self.name = name
        self.home = home
        self.script = script
        self.connect = connect
        self.extra_env = env or {}
        self.proc = None
        self.log_path = os.path.join(home, name + ".log")
        self._log_file = None

    def start(self):
        args = [self.binary, "-dedicated", "-disablestdincli", "-script", self.script]
        if self.connect:
            args += ["-connect", self.connect]
        env = dict(os.environ)
        # An isolated HOME keeps each instance's writes
        # out of the real user profile and out of each other's way.
        env["HOME"] = self.home
        env.update(self.extra_env)
        os.makedirs(self.home, exist_ok=True)
        self._log_file = open(self.log_path, "w")
        self.proc = subprocess.Popen(
            args, cwd=GAMEDIR, env=env,
            stdout=self._log_file, stderr=subprocess.STDOUT,
        )
        return self

    def read_log(self):
        try:
            with open(self.log_path) as fh:
                return fh.read()
        except FileNotFoundError:
            return ""

    def wait_for(self, marker, timeout):
        """Wait until *marker* appears in the log; return True, else False on timeout.

        Returns False fast if the process dies before the marker appears.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            if marker in self.read_log():
                return True
            if self.proc.poll() is not None:
                # Process exited; check the log one last time, then give up.
                return marker in self.read_log()
            time.sleep(0.2)
        return False

    def log_contains(self, text):
        return text in self.read_log()

    def stop(self):
        if self.proc and self.proc.poll() is None:
            # SIGKILL, not SIGTERM:
            # OLX's shutdown path is slow and can trip its own crash handler,
            # which only adds noise to the log.
            self.proc.kill()
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                pass
        if self._log_file:
            self._log_file.close()
            self._log_file = None


class NetworkGame:
    """A hosted dedicated server plus any clients connected to it.

    Instances share the read-only game data directory,
    but each gets its own isolated HOME under *base_home*.
    """

    # Fixed loopback port for the test server:
    # high enough to need no privileges, unlikely to clash on a CI runner.
    PORT = "23400"

    def __init__(self, binary, base_home):
        self.binary = binary
        self.base_home = base_home
        self.server = None
        self.clients = []

    def start_server(self, **settings):
        env = {"OLX_PORT": self.PORT}
        env.update({k: str(v) for k, v in settings.items()})
        self.server = OlxInstance(
            self.binary, "server", os.path.join(self.base_home, "server"),
            SERVER_CONTROL, env=env,
        ).start()
        return self.server

    def add_client(self, name):
        client = OlxInstance(
            self.binary, name, os.path.join(self.base_home, name),
            CLIENT_CONTROL, connect="127.0.0.1:" + self.PORT,
            env={"OLX_CLIENT_NAME": name},
        ).start()
        self.clients.append(client)
        return client

    def stop_all(self):
        for inst in self.clients + ([self.server] if self.server else []):
            inst.stop()
