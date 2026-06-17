#!/usr/bin/env bash
set -e

source /home/travis/zcw/1.2/setup.bash

PX4_BIN=/home/travis/zcw/1.2/PX4-Autopilot/build/px4_sitl_default/bin/px4
BUILD_DIR=/home/travis/zcw/1.2/PX4-Autopilot/build/px4_sitl_default
ROOTFS=$BUILD_DIR/rootfs

rm -rf "$ROOTFS/0" 2>/dev/null
cd "$ROOTFS"
exec "$PX4_BIN" -d "$BUILD_DIR/etc"
