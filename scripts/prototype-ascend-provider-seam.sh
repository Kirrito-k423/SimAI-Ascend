#!/bin/bash

# PROTOTYPE ONLY: compile and run the throwaway Ascend provider seam TUI.
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)
SOURCE="${ROOT_DIR}/astra-sim-alibabacloud/astra-sim/system/prototype/ascend-provider-seam/provider_seam_tui.cc"
BUILD_DIR=$(mktemp -d /tmp/simai-ascend-provider-prototype.XXXXXX)
trap 'rm -rf "${BUILD_DIR}"' EXIT

"${CXX:-c++}" -std=c++11 -Wall -Wextra -pedantic \
  -I"${ROOT_DIR}/astra-sim-alibabacloud" \
  "${SOURCE}" \
  -o "${BUILD_DIR}/provider-seam-prototype"

"${BUILD_DIR}/provider-seam-prototype" "$@"
