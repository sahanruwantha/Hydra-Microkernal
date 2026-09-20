#!/usr/bin/env bash
#
# amdrive.sh - helper library for driving the FSD-Amoeba VM through the
# QEMU monitor socket. Amoeba's console is VGA-only (no serial console),
# so everything goes through `sendkey` and screen state is read back with
# `screendump`.
#
# Source this, then use: mon, amtype, amkey, amshot, amwait
#
: "${SOCK:=/tmp/amoeba-mon.sock}"
: "${D:=/home/sahan/amoeba-ref}"

mon() { echo "$1" | socat - UNIX-CONNECT:"$SOCK" >/dev/null 2>&1; }

# Map one character to a QEMU keyname (US layout).
_keyname() {
    local c="$1"
    # The FSD-Amoeba console uses a GERMAN (QWERTZ) keymap, but QEMU's
    # `sendkey` names physical US positions. So we map each character to
    # the US key position that produces it on a German layout:
    #   y <-> z swapped;  '/' is shift-7;  '-' is the US slash key.
    case "$c" in
        y)    printf 'z' ;;
        z)    printf 'y' ;;
        Y)    printf 'shift-z' ;;
        Z)    printf 'shift-y' ;;
        [a-z0-9]) printf '%s' "$c" ;;
        [A-Z])    printf 'shift-%s' "$(printf '%s' "$c" | tr 'A-Z' 'a-z')" ;;
        ' ')  printf 'spc' ;;
        '.')  printf 'dot' ;;
        ',')  printf 'comma' ;;
        '/')  printf 'shift-7' ;;
        '-')  printf 'slash' ;;
        '_')  printf 'shift-slash' ;;
        ':')  printf 'shift-dot' ;;
        '<')  printf 'less' ;;
        '>')  printf 'shift-less' ;;
        '|')  printf 'alt_r-less' ;;
        ';')  printf 'shift-comma' ;;
        '=')  printf 'shift-0' ;;
        '(')  printf 'shift-8' ;;
        ')')  printf 'shift-9' ;;
        '!')  printf 'shift-1' ;;
        '"')  printf 'shift-2' ;;
        '$')  printf 'shift-4' ;;
        '%')  printf 'shift-5' ;;
        '&')  printf 'shift-6' ;;
        '?')  printf 'shift-minus' ;;
        '*')  printf 'shift-bracket_right' ;;
        '+')  printf 'bracket_right' ;;
        "'")  printf 'shift-backslash' ;;
        *)    printf '' ;;
    esac
}

# amtype "some command"   - type a string (no newline)
amtype() {
    local s="$1" i c k
    for ((i = 0; i < ${#s}; i++)); do
        c="${s:i:1}"
        k="$(_keyname "$c")"
        [[ -z "$k" ]] && continue
        mon "sendkey $k"
        sleep 0.035
    done
}

# amkey ret | amkey ctrl-c | ...
amkey() { mon "sendkey $1"; sleep 0.2; }

# amcmd "ls"  - type a command and press return
amcmd() { amtype "$1"; amkey ret; }

# amshot name  - screendump to $D/name.ppm and convert to png
amshot() {
    local n="$1"
    mon "screendump $D/$n.ppm"
    sleep 1
    python3 -c "
from PIL import Image
Image.open('$D/$n.ppm').save('$D/$n.png')
" 2>/dev/null
}
