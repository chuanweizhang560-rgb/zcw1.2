#!/bin/bash
# Phase 8: 4-UAV full-stack test on seed 42 desert world (flat center)
set -e

source /opt/ros/humble/setup.bash
source /home/travis/zcw/1.2/setup.bash

PX4_DIR=/home/travis/zcw/1.2/PX4-Autopilot
BUILD_DIR=$PX4_DIR/build/px4_sitl_default
WORLD=/home/travis/zcw/1.2/assets/worlds/generated/seed_42/desert_windfarm.world

export PX4_LOCKSTEP=0

cleanup() { kill $GZPID ${U_PIDS[@]} ${C_PIDS[@]} 2>/dev/null; }
trap cleanup EXIT

NUM=4
declare -A TCPS=( [0]=4560 [1]=4561 [2]=4562 [3]=4563 )
declare -A UDPS=( [0]=14560 [1]=14561 [2]=14562 [3]=14563 )
declare -A MAVROS_UDP=( [0]=14540 [1]=14541 [2]=14542 [3]=14543 )

# UAV start positions near base station (base at 0,0 elevation ~42m)
declare -A PX=( [0]=5 [1]=-5 [2]=10 [3]=-10 )
declare -A PY=( [0]=5 [1]=5 [2]=-5 [3]=-5 )

echo "=== Starting gzserver (seed 42) ==="
gzserver "$WORLD" --verbose > /tmp/gz_seed42_4uav.log 2>&1 &
GZPID=$!
sleep 8

echo "=== Spawning 4 Iris models ==="
for i in 0 1 2 3; do
    ID=$((i+1))
    python3 $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/scripts/jinja_gen.py \
        $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris/iris.sdf.jinja \
        $PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic \
        --mavlink_tcp_port ${TCPS[$i]} --mavlink_udp_port ${UDPS[$i]} --mavlink_id $ID \
        --output-file /tmp/uav${ID}_iris_seed42.sdf
    gz model --spawn-file /tmp/uav${ID}_iris_seed42.sdf --model-name uav${ID}_iris \
        -x ${PX[$i]} -y ${PY[$i]} -z 42.5
    echo "[UAV$ID] at (${PX[$i]}, ${PY[$i]}, 42.5)"
    sleep 2
done

echo "=== Starting PX4 ==="
U_PIDS=()
for i in 0 1 2 3; do
    ID=$((i+1))
    mkdir -p $BUILD_DIR/rootfs/uav${ID}
    cd $BUILD_DIR/rootfs/uav${ID}
    FLAGS="-d $BUILD_DIR/etc"
    [ "$i" -gt 0 ] && FLAGS="-i $i $FLAGS"
    PX4_SIM_MODEL=gazebo-classic_iris $BUILD_DIR/bin/px4 $FLAGS > out.log 2>&1 &
    U_PIDS+=($!)
    echo "[UAV$ID] PX4 $!"
    cd $PX4_DIR
    sleep 8
done

echo "=== Starting MAVROS ==="
source /home/travis/zcw/1.2/ros2_ws/install/local_setup.bash
for i in 0 1 2 3; do
    ID=$((i+1))
    SYS=$((i+1))
    nohup ros2 launch mavros px4.launch \
        fcu_url:="udp://:${MAVROS_UDP[$i]}@127.0.0.1:${MAVROS_UDP[$i]}" \
        namespace:=uav${ID} tgt_system:=${SYS} tgt_component:=1 \
        > /tmp/mavros_seed42_uav${ID}.log 2>&1 &
    echo "[UAV$ID] MAVROS"
    sleep 2
done

echo "=== Waiting for connections ==="
for s in $(seq 1 20); do
    ALL_OK=true
    for ID in 1 2 3 4; do
        if ! timeout 2 ros2 topic echo /uav${ID}/state --once 2>/dev/null | grep -q "connected: true"; then
            ALL_OK=false; break
        fi
    done
    $ALL_OK && echo "All connected after ${s}s" && break
    sleep 3
done

echo "=== Status ==="
for ID in 1 2 3 4; do
    echo -n "UAV$ID: "
    timeout 3 ros2 topic echo /uav${ID}/state --once 2>/dev/null | grep -E "connected:|armed:|mode:" | tr '\n' ' '
    echo
done

echo "=== Starting controllers ==="
C_PIDS=()
ros2 run zcw_offboard role_allocator --ros-args -p num_uavs:=4 &
C_PIDS+=($!); sleep 3

ros2 run zcw_offboard explore_control --ros-args -p own_ns:=uav1 -p scout_dist:=15.0 -p altitude:=20.0 &
C_PIDS+=($!); sleep 2

ros2 run zcw_offboard relay_control --ros-args -p own_ns:=uav2 -p inspect_ns:=uav1 &
C_PIDS+=($!); sleep 2

ros2 run zcw_offboard explore_control --ros-args -p own_ns:=uav3 -p scout_dist:=30.0 -p altitude:=25.0 &
C_PIDS+=($!); sleep 2

ros2 run zcw_offboard reserve_control --ros-args -p own_ns:=uav4 -p hold_x:=0.0 -p hold_y:=15.0 -p hold_z:=20.0 &
C_PIDS+=($!)

echo "=== INTEGRATION TEST ACTIVE ==="
echo "Press Ctrl+C to stop"
for r in $(seq 1 18); do sleep 10; echo "--- t=${r}0s ---"
    for ID in 1 2 3 4; do
        S=$(timeout 2 ros2 topic echo /uav${ID}/state --once 2>/dev/null | grep -E "connected:|armed:|mode:" | tr '\n' ' ')
        echo "  UAV$ID: $S"
    done
done
echo "=== TEST COMPLETE ==="
