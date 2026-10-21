#!/bin/sh
# Format all project C sources and headers with clang-format.
# Usage:
#   ./format.sh          rewrite files in place
#   ./format.sh --check  verify formatting without changing files

set -eu

cd "$(dirname "$0")"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is not installed." >&2
    exit 1
fi

case "${1:-}" in
    "")
        mode=write
        ;;
    --check)
        mode=check
        ;;
    *)
        echo "Usage: $0 [--check]" >&2
        exit 2
        ;;
esac

# The vendored Linux UAPI header must not be reformatted.
format_files() {
    find src -type f \( -name '*.c' -o -name '*.h' \) \
        ! -path 'src/opensuperclone/usbheaders.h' -print0 |
        xargs -0r clang-format "$@"
}

if [ "$mode" = check ]; then
    format_files --dry-run --Werror
    echo "Formatting check passed."
else
    format_files -i
    echo "Formatted all C sources and headers."
fi
