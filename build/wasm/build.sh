#!/usr/bin/env bash
# build.sh — produce a redistributable OpenLieroX Wasm bundle.
#
# Wraps build-wasm.sh (release build by default) and stages the
# resulting artefacts into <repo>/distrib/openlierox-wasm/, ready to
# upload to a static web host.
#
# The bundle requires the host to send COOP/COEP headers (the wasm
# build uses -pthread / SharedArrayBuffer). Three flavours of config
# ship in the bundle:
#   _headers              — Netlify / Cloudflare Pages
#   .htaccess             — Apache
#   coi-serviceworker.js  — fallback for hosts that can't set headers
#                           (notably GitHub Pages); registers a
#                           service worker that re-injects the
#                           Cross-Origin-* headers client-side.
# Other hosts (nginx, S3+CloudFront, ...) should set the same three
# headers natively if possible.
#
# Usage: ./build.sh [--debug] [extra args forwarded to build-wasm.sh]

set -euo pipefail

WASM_DIR="$(cd "$(dirname "$0")" && pwd)"
OLX_ROOT="$(cd "$WASM_DIR/../.." && pwd)"
DIST_DIR="$OLX_ROOT/distrib/openlierox-wasm"

BUILD_FLAVOR="--release"
FORWARD_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --debug)    BUILD_FLAVOR="--debug" ;;
        --release)  BUILD_FLAVOR="--release" ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *)          FORWARD_ARGS+=("$1") ;;
    esac
    shift
done

# ---------- build ---------------------------------------------------------

"$WASM_DIR/build-wasm.sh" "$BUILD_FLAVOR" "${FORWARD_ARGS[@]}"

OUT_DIR="$WASM_DIR/output"
for f in index.html openlierox.js openlierox.wasm openlierox.data; do
    if [ ! -f "$OUT_DIR/$f" ]; then
        echo "ERROR: expected artefact missing: $OUT_DIR/$f" >&2
        exit 1
    fi
done

# ---------- stage ---------------------------------------------------------

echo "Staging redistributable bundle into $DIST_DIR..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

cp -f "$OUT_DIR/index.html"      "$DIST_DIR/index.html"
cp -f "$OUT_DIR/openlierox.js"   "$DIST_DIR/openlierox.js"
cp -f "$OUT_DIR/openlierox.wasm" "$DIST_DIR/openlierox.wasm"
cp -f "$OUT_DIR/openlierox.data" "$DIST_DIR/openlierox.data"

# Cross-origin-isolation service-worker shim. Required for hosts
# that can't set COOP/COEP response headers (GitHub Pages). Harmless
# on hosts that do set the headers — the shim notices
# `crossOriginIsolated === true` and exits without registering.
SHIM_SRC="$WASM_DIR/shell/coi-serviceworker.js"
if [ ! -f "$SHIM_SRC" ]; then
    echo "ERROR: coi-serviceworker shim missing at $SHIM_SRC" >&2
    exit 1
fi
cp -f "$SHIM_SRC" "$DIST_DIR/coi-serviceworker.js"
[ -f "$WASM_DIR/shell/coi-serviceworker.LICENSE" ] && \
    cp -f "$WASM_DIR/shell/coi-serviceworker.LICENSE" \
          "$DIST_DIR/coi-serviceworker.LICENSE"

# PWA assets: web app manifest + icons referenced by the shell's <head>.
# These make the bundle installable as a standalone web app out of the box
# (the shell's "Install web app" button needs a manifest to get a real
# install prompt). The manifest uses relative start_url/scope, so it works
# wherever the bundle is hosted.
for asset in manifest.webmanifest icon-256.png icon-512.png; do
    if [ ! -f "$WASM_DIR/shell/$asset" ]; then
        echo "ERROR: shell asset missing: $WASM_DIR/shell/$asset" >&2
        exit 1
    fi
    cp -f "$WASM_DIR/shell/$asset" "$DIST_DIR/$asset"
done

# Inject the shim's <script> into the staged index.html, immediately
# before </head>, so it registers before the emscripten loader runs.
# The shim must be loaded as a regular script (no defer / async /
# type=module) because it relies on document.currentScript.src to
# self-register as the service worker.
python3 - "$DIST_DIR/index.html" <<'PY'
import sys, pathlib
p = pathlib.Path(sys.argv[1])
html = p.read_text()
tag = '  <script src="coi-serviceworker.js"></script>\n'
if tag.strip() in html:
    sys.exit(0)
needle = "</head>"
i = html.find(needle)
if i == -1:
    sys.stderr.write(f"ERROR: no </head> in {p}\n")
    sys.exit(1)
p.write_text(html[:i] + tag + html[i:])
PY

# Netlify / Cloudflare Pages headers config.
cat > "$DIST_DIR/_headers" <<'EOF'
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
  Cross-Origin-Resource-Policy: same-origin
EOF

# Apache headers + correct MIME types.
cat > "$DIST_DIR/.htaccess" <<'EOF'
<IfModule mod_headers.c>
    Header set Cross-Origin-Opener-Policy   "same-origin"
    Header set Cross-Origin-Embedder-Policy "require-corp"
    Header set Cross-Origin-Resource-Policy "same-origin"
</IfModule>

AddType application/wasm         .wasm
AddType application/octet-stream .data
AddType application/javascript   .js
EOF

# Machine-readable build metadata. Lets a downstream deployer (e.g. the
# website's update step) read the exact version/commit a bundle came from
# instead of eyeballing the release title, and script a versioned rollout
# (name the target folder, record provenance) without guessing.
OLX_VERSION="$("$OLX_ROOT/get_version.sh" 2>/dev/null || echo unknown)"
OLX_COMMIT="$(git -C "$OLX_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
OLX_COMMIT_SHORT="$(git -C "$OLX_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
OLX_COMMIT_DATE="$(git -C "$OLX_ROOT" show -s --format=%cI HEAD 2>/dev/null || echo unknown)"
OLX_VERSION="$OLX_VERSION" OLX_COMMIT="$OLX_COMMIT" \
OLX_COMMIT_SHORT="$OLX_COMMIT_SHORT" OLX_COMMIT_DATE="$OLX_COMMIT_DATE" \
python3 - "$DIST_DIR/build-info.json" <<'PY'
import json, os, sys
json.dump({
    "name": "openlierox-wasm",
    "version": os.environ["OLX_VERSION"],
    "commit": os.environ["OLX_COMMIT"],
    "commitShort": os.environ["OLX_COMMIT_SHORT"],
    "commitDate": os.environ["OLX_COMMIT_DATE"],
    # The web entry point and the engine artefacts a deployer must publish.
    "entry": "index.html",
    "engineFiles": ["openlierox.js", "openlierox.wasm", "openlierox.data"],
    "files": [
        "index.html", "openlierox.js", "openlierox.wasm", "openlierox.data",
        "coi-serviceworker.js", "coi-serviceworker.LICENSE",
        "manifest.webmanifest", "icon-256.png", "icon-512.png",
        "_headers", ".htaccess",
    ],
}, open(sys.argv[1], "w"), indent=2)
open(sys.argv[1], "a").write("\n")
PY

# ---------- summary -------------------------------------------------------

echo
echo "Bundle ready: $DIST_DIR"
ls -lh "$DIST_DIR"
TOTAL=$(du -sh "$DIST_DIR" | cut -f1)
echo
echo "Total size: $TOTAL"
echo "Upload the contents of $DIST_DIR to your web host."
echo
echo "Cross-origin isolation (required for SharedArrayBuffer/pthread):"
echo "  - Hosts with header config: covered by _headers (Netlify / CF Pages)"
echo "    and .htaccess (Apache); set the same headers natively elsewhere:"
echo "      Cross-Origin-Opener-Policy:   same-origin"
echo "      Cross-Origin-Embedder-Policy: require-corp"
echo "      Cross-Origin-Resource-Policy: same-origin"
echo "  - Hosts without header config (GitHub Pages): coi-serviceworker.js"
echo "    is wired into index.html and provides the headers client-side."
echo "    First load triggers a one-time reload to activate the worker."
