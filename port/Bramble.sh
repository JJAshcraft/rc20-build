#!/bin/bash
# Bramble launcher for the R36S and other RK3326 handhelds running
# ArkOS, dArkOS / dArkOSRE or another PortMaster-compatible firmware.
# Copy this file and the bramble/ folder into /roms/ports (or roms2/ports).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

# PortMaster, when present, knows this device's gamepad layout for SDL.
if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  get_controls
fi

GAMEDIR="$(cd "$(dirname "$0")/bramble" && pwd)"
cd "$GAMEDIR" || exit 1
chmod +x ./bramble.aarch64 2>/dev/null

if [ -n "$sdl_controllerconfig" ]; then
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi

./bramble.aarch64 --data "$GAMEDIR" > "$GAMEDIR/log.txt" 2>&1

printf "\033c" > /dev/tty1 2>/dev/null
