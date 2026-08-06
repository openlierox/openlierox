#!/bin/sh
set -e

# Path (relative to the gamedir, WITHOUT the .py suffix) of the dedicated-server
# control config. Override at run time, e.g.:
#   docker run -e OLX_DED_CONFIG=cfg/dedicated_config_lx56 ...
: "${OLX_DED_CONFIG:=cfg/dedicated_config}"

cd /opt/openlierox

# -dedicated : headless server mode (no graphics, no sound)
# -exec      : run a console command at startup. Here it loads the Python
#              automation script, which cycles lobby -> game, rotates levels and
#              mods, handles voting and admin/user chat commands, etc.
exec openlierox -dedicated -exec "script dedicated_control ${OLX_DED_CONFIG}"
