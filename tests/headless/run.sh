#!/bin/bash
# Build openlierox if needed, then run the headless test suite.
# Extra arguments are passed through to pytest, e.g.:
#   ./tests/headless/run.sh -k network -v
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

if [ ! -x "$ROOT/build-output/bin/openlierox" ] && [ -z "${OLX_BINARY:-}" ]; then
    echo ">>> openlierox not built yet; building ..."
    "$ROOT/tests/headless/build.sh"
fi

PYTHON="${PYTHON:-python3}"
cd "$ROOT/tests/headless"
exec "$PYTHON" -m pytest "$@"
