#!/bin/sh
# Print the reviewed SDF installation proposal. This is a dated snapshot,
# not a live test result; review docs/outreach/sdf/requests.txt before reuse.
set -eu
case "$#:$*" in
    '0:') ;;
    '1:-h'|'1:--help')
        echo 'Usage: scripts/gen-bboard-post.sh'
        echo 'Prints docs/outreach/sdf/requests.txt; does not post it.'
        exit 0 ;;
    *) echo 'Usage: scripts/gen-bboard-post.sh (no version/count overrides)' >&2
       exit 2 ;;
esac
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cat "$repo_root/docs/outreach/sdf/requests.txt"
