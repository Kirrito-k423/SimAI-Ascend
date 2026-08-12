#!/bin/bash

# PROTOTYPE ONLY: run the hierarchical A2A projection TUI.
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

exec python3 "${ROOT_DIR}/prototypes/hierarchical_a2a_projection/tui.py" "$@"
