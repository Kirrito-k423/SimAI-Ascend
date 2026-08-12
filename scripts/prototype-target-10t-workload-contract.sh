#!/bin/bash

# PROTOTYPE ONLY: run the Target 10T Workload Contract TUI.
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

exec python3 "${ROOT_DIR}/prototypes/target_10t_workload_contract/tui.py" "$@"
