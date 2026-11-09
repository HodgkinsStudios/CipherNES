#!/bin/sh
# Created by Jacob Hodgkins
set -eu

APP="/usr/lib/ciphernes/CipherNES"
# The native executable creates the OS-specific folders on first launch.
# The wrapper only makes command-line paths absolute before that chdir.

abs_path() {
    realpath -m -- "$1"
}

case "${1:-}" in
    --help)
        exec "$APP" "$@"
        ;;
    --validate|--benchmark)
        mode="$1"
        shift
        if [ "$#" -gt 0 ]; then
            rom="$(abs_path "$1")"
            shift
                exec "$APP" "$mode" "$rom" "$@"
        fi
        exec "$APP" "$mode"
        ;;
    --ui-preview)
        mode="$1"
        shift
        if [ "$#" -gt 0 ]; then
            output="$(abs_path "$1")"
            shift
            if [ "$#" -gt 0 ]; then
                rom="$(abs_path "$1")"
                shift
                        exec "$APP" "$mode" "$output" "$rom" "$@"
            fi
                exec "$APP" "$mode" "$output"
        fi
        exec "$APP" "$mode"
        ;;
    --controller-test)
        exec "$APP" "$@"
        ;;
    --*)
        exec "$APP" "$@"
        ;;
    "")
        exec "$APP"
        ;;
    *)
        rom="$(abs_path "$1")"
        shift
        exec "$APP" "$rom" "$@"
        ;;
esac
