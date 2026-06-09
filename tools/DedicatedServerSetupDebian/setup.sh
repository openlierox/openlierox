#!/bin/bash

set -euo pipefail

sudo bash -c "apt-get install -y build-essential git psmisc cmake \
  libsdl1.2-dev libsdl-mixer1.2-dev libsdl-image1.2-dev \
  libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev libgd-dev \
  zlib1g-dev libzip-dev libxml2-dev libcurl4-gnutls-dev \
  libboost-dev libboost-system-dev libopenal-dev libalut-dev libvorbis-dev \
  zlib1g-dev libreadline-dev && \
  apt-get build-dep -y python3"

curl -fsSL https://pyenv.run | bash

export PYENV_ROOT="$HOME/.pyenv"
[[ -d $PYENV_ROOT/bin ]] && export PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init - bash)"

pyenv install 2.7.18

cmake -D HAWKNL_BUILTIN=1 -D DEBUG=0 -D DEDICATED_ONLY=1 .

make -j `nproc`

cd tools/UDPMasterServer
./compile.sh
cd ../..

{
  crontab -l || echo ''
  echo ''
  echo '@reboot while true; do $HOME/openlierox/tools/UDPMasterServer/bin/udpmasterserver; sleep 1; done'
  echo '@reboot while true; do $HOME/openlierox/tools/UDPMasterServer/bin/udpmasterserver -6; sleep 1; done'
  echo '@reboot export PATH=$HOME/.pyenv/versions/2.7.18/bin:$PATH; cd $HOME/openlierox/share/gamedir; while true; do ../../bin/openlierox 2>&1 | logger --tag openlierox; sleep 10; done'
  echo '0 */6 * * * killall openlierox; sleep 1; killall -9 openlierox'
  echo '0 2 * * * find $HOME/.OpenLieroX/logs -mtime +7 -type f -delete'
} | crontab -

echo "Setup done, now reboot your server"
