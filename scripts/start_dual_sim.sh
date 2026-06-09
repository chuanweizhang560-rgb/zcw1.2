#!/bin/bash
# Phase 5: Dual-UAV v2 - stable version
# Fixed: gzserver without camera plugin, PX4_LOCKSTEP=0, controllers handle OFFBOARD
set -e

source /opt/ros/humble/setup.bash
source /home/travis/zcw/1.2/setup.bash

PX4_DIR=/home/travis/zcw/1.2/PX4-Autopilot
BUILD_DIR=$PX4_DIR/build/px4_sitl_default
WORLD=/home/travis/zcw/1.2/assets/worlds/cable_inspection.world
export PX4_LOCKSTEP=0

STEP=/tmp/zcw_dual_step.txt
sp() { echo "[$1] $2"; echo "$1" > $STEP; }

cleanup() {
    kill $GZPID $U1P $U2P 2>/dev/null
    pkill -f "mavros_node" 2>/dev/null
    pkill -f "cable_tracker|relay_control" 2>/dev/null
    pkill -f "ros2 topic pub.*setpoint" 2>/dev/null
}
trap cleanup EXIT

sp 1 "Starting gzserver..."
gzserver "$WORLD" --verbose > /tmp/gz_dual.log 2>&1 &
GZPID=$!
sleep 10

# Generate SDFs
python3 $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/scripts/jinja_gen.py \
    $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris/iris.sdf.jinja \
    $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic \
    --mavlink_tcp_port 4560 --mavlink_udp_port 14560 --mavlink_id 1 \
    --output-file /tmp/uav1_iris.sdf
python3 $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/scripts/jinja_gen.py \
    $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris/iris.sdf.jinja \
    $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic \
    --mavlink_tcp_port 4561 --mavlink_udp_port 14561 --mavlink_id 2 \
    --output-file /tmp/uav2_iris.sdf

sp 2 "Spawning both models..."
gz model --spawn-file /tmp/uav1_iris.sdf --model-name uav1_iris -x -30.0 -y 0 -z 0.83
gz model --spawn-file /tmp/uav2_iris.sdf --model-name uav2_iris -x 30.0 -y 5.0 -z 0.83
sleep 3

sp 3 "Starting PX4 instances (LOCKSTEP=0)..."
mkdir -p $BUILD_DIR/rootfs/uav1
cd $BUILD_DIR/rootfs/uav1
PX4_SIM_MODEL=gazebo-classic_iris $BUILD_DIR/bin/px4 -d $BUILD_DIR/etc > out.log 2>&1 &
U1P=$!
cd $PX4_DIR
sleep 8

mkdir -p $BUILD_DIR/rootfs/uav2
cd $BUILD_DIR/rootfs/uav2
PX4_SIM_MODEL=gazebo-classic_iris $BUILD_DIR/bin/px4 -i 1 -d $BUILD_DIR/etc > out.log 2>&1 &
U2P=$!
cd $PX4_DIR
sleep 10

sp 4 "Starting MAVROS..."
source /home/travis/zcw/1.2/ros2_ws/install/local_setup.bash
nohup ros2 launch mavros px4.launch fcu_url:="udp://:14540@127.0.0.1:14540" namespace:=uav1 \
    tgt_system:=1 tgt_component:=1 > /tmp/mavros_u1.log 2>&1 &
sleep 2
nohup ros2 launch mavros px4.launch fcu_url:="udp://:14541@127.0.0.1:14541" namespace:=uav2 \
    tgt_system:=2 tgt_component:=1 > /tmp/mavros_u2.log 2>&1 &

sp 5 "Waiting for MAVROS connection..."
for i in $(seq 1 15); do
    u1=$(timeout 2 ros2 topic echo /uav1/state --once 2>/dev/null | grep -c "connected: true" || echo 0)
    u2=$(timeout 2 ros2 topic echo /uav2/state --once 2>/dev/null | grep -c "connected: true" || echo 0)
    [ "$u1" -gt 0 ] && [ "$u2" -gt 0 ] && sp 6 "Both connected!" && break
    sleep 2
done

# Final connectivity check
echo "=== STATE ==="
echo "UAV1:"; timeout 3 ros2 topic echo /uav1/state --once 2>/dev/null | grep -E "connected:|mode:"
echo "UAV2:"; timeout 3 ros2 topic echo /uav2/state --once 2>/dev/null | grep -E "connected:|mode:"

# ===== Key change: start controllers, let THEM handle OFFBOARD+ARM =====
# Both publish setpoints from t=0, wait for connection, then auto OFFBOARD+ARM
sp 7 "Starting cable_tracker (INSPECT on uav1)..."
ros2 run zcw_offboard cable_tracker --ros-args -p mavros_ns:=uav1 -p tower_x1:=-30.0 -p tower_x2:=30.0 &
C1=$!
sleep 8

sp 8 "Starting relay_control (RELAY on uav2)..."
ros2 run zcw_offboard relay_control --ros-args -p own_ns:=uav2 -p inspect_ns:=uav1 &
C2=$!

sp 9 "DUAL-UAV ACTIVE: INSPECT(uav1) + RELAY(uav2)"
echo "Monitoring... Press Ctrl+C to stop"

wait $C1 $C2
