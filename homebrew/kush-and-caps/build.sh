#!/bin/sh
set -e
cd "$(dirname "$0")"
if [ -z "$OO_PS4_TOOLCHAIN" ]; then
  echo "Set OO_PS4_TOOLCHAIN to your OpenOrbis PS4Toolchain directory" >&2
  exit 1
fi
if [ ! -f sce_module/libc.prx ] || [ ! -f sce_sys/about/right.sprx ]; then
  echo "Copy OpenOrbis sample libc.prx and libSceFios2.prx into sce_module/" >&2
  echo "and right.sprx into sce_sys/about/ (they ship with the toolchain, not this repo)." >&2
  exit 1
fi
make
