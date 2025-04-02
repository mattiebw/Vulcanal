#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SCRIPT_DIR
"$SCRIPT_DIR/Vulcanal"
