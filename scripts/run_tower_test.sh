#!/bin/bash
set -e

echo "=== ZCW Phase 3: TL Tower World Test ==="

# --- Cleanup ---
killall -9 gzserver gzclient px4 2>/dev/null || true
sleep 1

# --- Source env ---
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh

# --- PX4 environment ---
export PX4_ROOT=/home/travis/zcw/1.2/PX4-Autopilot
export PX4_SIM_MODEL=gazebo-classic_iris
export PX4_SITL_WORLD=tower

source ${PX4_ROOT}/Tools/simulation/gazebo-classic/setup_gazebo.bash ${PX4_ROOT} ${PX4_ROOT}/build/px4_sitl_default

export OGRE_RTT_MODE=Copy

echo "=== 1. Starting Gazebo server with tower.world ==="
cd ${PX4_ROOT}
HEADLESS=1 make px4_sitl gazebo-classic_iris &
PX4_PID=$!
echo "PX4 PID: $PX4_PID"

# Wait for Gazebo master and PX4
sleep 15

echo "=== 2. Checking processes ==="
echo -n "gzserver: "; pgrep gzserver | wc -l
echo -n "px4: "; pgrep px4 | wc -l
echo -n "gazebo master: "; pgrep -f "gazebo.*master" | wc -l

echo "=== 3. Starting gzclient ==="
gzclient --verbose &
GZCLIENT_PID=$!
echo "gzclient PID: $GZCLIENT_PID"

sleep 3

echo "=== 4. Done ==="
echo "Check the Gazebo window for tower models."
echo ""
echo "Press ENTER to shut down..."
read -r

echo "=== Shutting down ==="
kill $PX4_PID 2>/dev/null || true
killall -9 gzserver gzclient 2>/dev/null || true
echo "Done."
