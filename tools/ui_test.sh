#!/bin/sh
# Drives the real app headless (offscreen video, no audio) with scripted button
# presses and checks the state it reports. Usage: tools/ui_test.sh [build/bramble]
BIN=${1:-build/bramble}
DATA=$(mktemp -d)
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
fail=0

check() { # name script expected-substring...
    name=$1; script=$2; shift 2
    out=$("$BIN" --data "$DATA" --shot "$DATA/shot.bmp" --script "$script" 2>&1)
    for want in "$@"; do
        case "$out" in
        *"$want"*) ;;
        *) echo "FAIL $name: wanted '$want' in: $out"; fail=1; return ;;
        esac
    done
    echo "ok   $name"
}

check "starts on moss pad"        ""                         "page=OSC A" "mode=edit" "preset=2"
check "pages forward"             "r1 r1 r1"                 "page=FILTER" "sel=f_type" "value=LP 24"
check "pages wrap backward"       "l1"                       "page=INPUT TEST"
check "row select + adjust"       "r1 r1 r1 down right right" "sel=f_cut" "value=957 HZ"
check "enum wraps"                "down left left left"      "sel=a_table" "value=GRIT"
check "reset to default"          "down y"                   "sel=a_table" "value=BASIC"
check "play mode"                 "select"                   "mode=play"
check "octave up"                 "select r1"                "oct=5"
check "hold toggle"               "select l2"                "hold=1"
check "arp toggle"                "select r2"                "arp=1"
check "next sound"                "r3"                       "preset=3"
check "prev sound"                "l3"                       "preset=1"
check "preset page load"          "start down a"             "page=PRESETS" "preset=3"
check "preset save"               "start left left x x"      "preset="
grep -q '^\[18\] USER 18' "$DATA/bramble_presets.txt" && echo "ok   preset file written" \
    || { echo "FAIL preset file: $(head -3 "$DATA/bramble_presets.txt" 2>&1)"; fail=1; }
check "saved slot persists"       "start left left a"        "preset=18"
check "notes in edit mode"        "+a -a +x -x"              "mode=edit"

rm -rf "$DATA"
[ $fail = 0 ] && echo "all ui tests passed"
exit $fail
