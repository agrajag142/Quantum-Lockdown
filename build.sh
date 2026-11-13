#!/usr/bin/env bash
# Builds QUANTUM LOCKDOWN (the full-screen game) as a PS5 app folder and ZIP, the same
# way app/build.sh builds Hello PS5 and silo/build.sh NUCLEAR MISSION (same
# kit, same SDK: run tools/setup.sh once first).
#
#   mission2/build.sh                    -> build/mission2/PPSA99903/ and PPSA99903.zip
#   mission2/build.sh deploy <PS5 IP>    -> also copies it to /data/homebrew over FTP
#   mission2/build.sh sim [script]       -> the PC test build instead (no PS5 needed):
#                                           plays a scripted controller, saves screenshots
set -euo pipefail

usage() { echo "usage: mission2/build.sh [deploy <PS5 IP> | sim [script.txt]]" >&2; exit 2; }
deploy_host=
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(dirname -- "$here")
case ${1:-} in
    "") [[ $# -eq 0 ]] || usage ;;
    deploy) [[ $# -eq 2 && -n $2 ]] || usage; deploy_host=$2 ;;
    sim)
        out=$repo/build/mission2-sim
        mkdir -p "$out/shots"
        gcc -O2 -c "$here/src/micromod.c" -o "$out/micromod.o"
        gcc -O2 -c "$here/src/micromod2.c" -o "$out/micromod2.o"
        gcc -O2 -c "$here/src/micromod3.c" -o "$out/micromod3.o"
        gcc -O2 -c "$here/src/micromod4.c" -o "$out/micromod4.o"
        gcc -O2 -c "$here/src/micromod5.c" -o "$out/micromod5.o"
        gcc -O2 -c "$here/src/micromod6.c" -o "$out/micromod6.o"
        g++ -std=c++20 -O2 -I"$here/src" "$here/src/game.cpp" "$here/src/gfx.cpp" "$here/sim/sim_main.cpp" "$out/micromod.o" "$out/micromod2.o" "$out/micromod3.o" "$out/micromod4.o" "$out/micromod5.o" "$out/micromod6.o" -o "$out/sim"
        "$out/sim" "${2:-$here/sim/tour.txt}" "$out/shots" "$here/assets/elevator.mod" "$here/assets/stage1.mod" "$here/assets/title.mod" "$here/assets/stage2.mod" "$here/assets/gameovergood.mod" "$here/assets/gameoverbad.mod"
        if python3 -c 'import PIL' 2>/dev/null; then
            python3 -c 'import sys, glob, os
from PIL import Image
for f in glob.glob(os.path.join(sys.argv[1], "*.ppm")):
    Image.open(f).save(f[:-4] + ".png"); os.remove(f)' "$out/shots"
        fi
        echo "Screenshots: build/mission2-sim/shots"
        exit 0 ;;
    *) usage ;;
esac

home=${PS5_STARTER_HOME:-$HOME/.ps5-starter}
kit=$home/kit-work
sdk=${PS5_PAYLOAD_SDK:-$home/ps5-payload-sdk}

# 1. The kit, copied to the work folder whenever kit/ changed (as app/build.sh does).
stamp=$(cd "$repo/kit" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -c1-16)
if [[ ! -f $kit/.kit-stamp || $(cat "$kit/.kit-stamp") != "$stamp" ]]; then
    echo "Preparing the kit..."
    mkdir -p "$kit"
    find "$kit" -mindepth 1 -maxdepth 1 ! -name .deps -exec rm -rf {} +
    cp -r "$repo/kit/." "$kit/"
    echo "$stamp" > "$kit/.kit-stamp"
fi
kit_sdk=$kit/.deps/native/ps5-payload-sdk
[[ ! -L $kit_sdk || -e $kit_sdk ]] || rm -f "$kit_sdk"
if [[ ! -e $kit_sdk && -x $sdk/bin/prospero-lld ]]; then
    mkdir -p "$kit/.deps/native"
    ln -s "$sdk" "$kit_sdk"
fi

# 2. The game's files beside the kit's own (in kit-work/mission2: Hello PS5's
#    kit-work/starter and NUCLEAR MISSION's kit-work/silo are left alone).
#    Only src/ is compiled: the PC test build (sim/) stays out.
rm -rf "$kit/mission2"
mkdir -p "$kit/mission2"
cp -r "$here/src" "$here/sce_sys" "$here/assets" "$kit/mission2/"
find "$kit/mission2/assets" -name 'README*' -delete
bash "$kit/tools/validate-assets.sh" "$kit/mission2/sce_sys"

# 3. Build. The kit compiles every .c/.cpp under mission2/src.
vars=(APP_SOURCE_DIR=mission2/src APP_PARAM=mission2/sce_sys/param.json
      APP_SCE_SYS=mission2/sce_sys APP_ASSETS=mission2/assets)
if [[ -n $deploy_host ]]; then
    make -C "$kit" "${vars[@]}" deploy PS5_HOST="$deploy_host"
else
    make -C "$kit" "${vars[@]}"
fi

# 4. The result, next to the sources.
title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$here/sce_sys/param.json")
rm -rf "$repo/build/mission2"
mkdir -p "$repo/build/mission2"
cp -r "$kit/dist/$title" "$kit/dist/$title.zip" "$repo/build/mission2/"
echo
echo "App folder: build/mission2/$title"
echo "ZIP:        build/mission2/$title.zip"
