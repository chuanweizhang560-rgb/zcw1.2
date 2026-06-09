#!/bin/bash
# Phase 6: 4-UAV simulation base
# 1 gzserver + 4 Iris + 4 PX4 (-i 0..3) + 4 MAVROS (uav1..uav4)
set -e

source /opt/ros/humble/setup.bash
source /home/travis/zcw/1.2/setup.bash

PX4_DIR=/home/travis/zcw/1.2/PX4-Autopilot
BUILD_DIR=$PX4_DIR/build/px4_sitl_default
WORLD=/home/travis/zcw/1.2/assets/worlds/cable_inspection.world
export PX4_LOCKSTEP=0

STEP=/tmp/zcw_4uav_step.txt
sp() { echo "[$1] $2"; echo "$1" > $STEP; }

cleanup() {
    kill $GZPID ${U_PIDS[@]} 2>/dev/null
    pkill -f "mavros_node" 2>/dev/null
    pkill -f "ros2 run zcw" 2>/dev/null
}
trap cleanup EXIT

NUM=4
declare -A TCPS=( [0]=4560 [1]=4561 [2]=4562 [3]=4563 )
declare -A UDPS=( [0]=14560 [1]=14561 [2]=14562 [3]=14563 )
declare -A MAVROS_UDP=( [0]=14540 [1]=14541 [2]=14542 [3]=14543 )

# Positions: uav1=INSPECT, uav2=RELAY, uav3=EXPLORE, uav4=RESERVE
declare -A PX=( [0]=-30 [1]=30 [2]=0 [3]=0 )
declare -A PY=( [0]=0 [1]=5 [2]=-20 [3]=20 )

sp 1 "Starting gzserver..."
gzserver "$WORLD" --verbose > /tmp/gz_4uav.log 2>&1 &
GZPID=$!
sleep 10

sp 2 "Generating SDF models..."
for i in $(seq 0 $((NUM-1))); do
    ID=$((i+1))
    python3 $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/scripts/jinja_gen.py \
        $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris/iris.sdf.jinja \
        $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic \
        --mavlink_tcp_port ${TCPS[$i]} --mavlink_udp_port ${UDPS[$i]} --mavlink_id $ID \
        --output-file /tmp/uav${ID}_iris.sdf
    gz model --spawn-file /tmp/uav${ID}_iris.sdf --model-name uav${ID}_iris \
        -x ${PX[$i]} -y ${PY[$i]} -z 0.83
    echo "[UAV$ID] spawned at (${PX[$i]}, ${PY[$i]})"
    sleep 2
done

sp 3 "Starting PX4 instances..."
U_PIDS=()
for i in $(seq 0 $((NUM-1))); do
    ID=$((i+1))
    mkdir -p $BUILD_DIR/rootfs/uav${ID}
    cd $BUILD_DIR/rootfs/uav${ID}
    FLAGS="-d $BUILD_DIR/etc"
    [ "$i" -gt 0 ] && FLAGS="-i $i $FLAGS"
    PX4_SIM_MODEL=gazebo-classic_iris $BUILD_DIR/bin/px4 $FLAGS > out.log 2>&1 &
    U_PIDS+=($!)
    echo "[UAV$ID] PX4 PID=$!"
    cd $PX4_DIR
    sleep 8
done

sp 4 "Starting MAVROS..."
source /home/travis/zcw/1.2/ros2_ws/install/local_setup.bash
for i in $(seq 0 $((NUM-1))); do
    ID=$((i+1))
    SYS=$((i+1))
    nohup ros2 launch mavros px4.launch \
        fcu_url:="udp://:${MAVROS_UDP[$i]}@127.0.0.1:${MAVROS_UDP[$i]}" \
        namespace:=uav${ID} tgt_system:=${SYS} tgt_component:=1 \
        > /tmp/mavros_uav${ID}.log 2>&1 &
    echo "[UAV$ID] MAVROS started (port ${MAVROS_UDP[$i]})"
    sleep 2
done

sp 5 "Waiting for connections..."
for i in $(seq 1 20); do
    ALL_OK=true
    for ID in $(seq 1 $NUM); do
        if ! timeout 2 ros2 topic echo /uav${ID}/state --once 2>/dev/null | grep -q "connected: true"; then
            ALL_OK=false; break
        fi
    done
    $ALL_OK && sp 6 "All $NUM UAVs connected after ${i}s!" && break
    sleep 3
done

# Final state dump
for ID in $(seq 1 $NUM); do
    echo "=== UAV$ID ==="
    timeout 3 ros2 topic echo /uav${ID}/state --once 2>/dev/null | grep -E "connected:|mode:" || echo "NOT CONNECTED"
done >> /tmp/4uav_state.txt
cat /tmp/4uav_state.txt

if ! grep -q "connected: true" /tmp/4uav_state.txt; then
    sp -1 "CONNECTION FAILED"; exit 1
fi

sp 7 "4-UAV BASE READY. Starting role allocator + controllers..."

# Role allocator (assigns roles once per second)
ros2 run zcw_offboard role_allocator --ros-args -p num_uavs:=4 &
C_PIDS+=($!)
sleep 3

# INSPECT (uav1) - cable tracking
ros2 run zcw_offboard cable_tracker --ros-args -p mavros_ns:=uav1 -p tower_x1:=-30.0 -p tower_x2:=30.0 &
C_PIDS+=($!)
sleep 2

# RELAY (uav2) - follow INSPECT
ros2 run zcw_offboard relay_control --ros-args -p own_ns:=uav2 -p inspect_ns:=uav1 &
C_PIDS+=($!)
sleep 2

# EXPLORE (uav3) - scout waypoints
ros2 run zcw_offboard explore_control --ros-args -p own_ns:=uav3 -p scout_dist:=20.0 -p altitude:=20.0 &
C_PIDS+=($!)
sleep 2

# RESERVE (uav4) - hold at standby position
ros2 run zcw_offboard reserve_control --ros-args -p own_ns:=uav4 -p hold_x:=0.0 -p hold_y:=20.0 -p hold_z:=20.0 &
C_PIDS+=($!)

sp 99 "4-UAV ACTIVE: INSPECT(uav1) RELAY(uav2) EXPLORE(uav3) RESERVE(uav4)"
echo "Phase 6 running. Press Ctrl+C to stop."

wait ${C_PIDS[@]}
