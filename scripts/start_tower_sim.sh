#!/bin/bash
# Start PX4 + Gazebo with TL tower world + gzclient
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PX4_ROOT="${PROJECT_ROOT}/PX4-Autopilot"

echo "=== ZCW Tower Simulation ==="

# Cleanup
killall -9 gzserver gzclient px4 2>/dev/null || true
sleep 1

# Source env (dual source order)
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh

# Export env
export PX4_SITL_WORLD=tower
export OGRE_RTT_MODE=Copy
export LIBGL_ALWAYS_SOFTWARE=1

source "${PX4_ROOT}/Tools/simulation/gazebo-classic/setup_gazebo.bash" "${PX4_ROOT}" "${PX4_ROOT}/build/px4_sitl_default"

echo ""
echo "=== 1. Starting PX4 SITL (gazebo-classic iris + tower world) ==="
cd "${PX4_ROOT}"
HEADLESS=1 make px4_sitl gazebo-classic_iris &
PX4_PID=$!
echo "PX4 PID: ${PX4_PID}"

# Wait for PX4 + gzserver to be ready
sleep 15

echo ""
echo "=== 2. Starting gzclient (Gazebo GUI) ==="
gzclient --verbose &
GZCLIENT_PID=$!
echo "gzclient PID: ${GZCLIENT_PID}"

echo ""
echo "=== 3. Checking status ==="
pgrep -a gzserver
pgrep -a px4
ss -tulpn | grep 14580 || echo "(mavlink port waiting...)"
echo ""
echo "Simulation is running. Close this terminal to stop."
echo "Or press Ctrl+C to stop."

# Wait forever (or until interrupted)
wait
