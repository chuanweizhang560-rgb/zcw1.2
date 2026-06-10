#!/bin/bash
# Phase 8: FAST-LIVO2 全栈验证脚本
# 1 gzserver (cable_inspection.world) + iris_stereo_velodyne + PX4 + MAVROS + FAST-LIVO2
set -e

source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
source /home/travis/zcw/1.2/setup.bash

PX4_DIR=/home/travis/zcw/1.2/PX4-Autopilot
BUILD_DIR=$PX4_DIR/build/px4_sitl_default
WORLD=/home/travis/zcw/1.2/assets/worlds/cable_inspection.world
SDF=/home/travis/zcw/1.2/PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris_stereo_velodyne/model.sdf
export PX4_LOCKSTEP=0

cleanup() {
    echo "=== CLEANUP ==="
    kill $GZPID $PX4PID $MAVROSPID $LIVOPID 2>/dev/null
    pkill -f gzserver 2>/dev/null
    pkill -f mavros_node 2>/dev/null
    pkill -f fast_livo2 2>/dev/null
    sleep 1
}
trap cleanup EXIT

echo "=== 1. Starting gzserver ==="
killall -9 gzserver gzclient 2>/dev/null || true
sleep 1
gzserver "$WORLD" --verbose > /tmp/gz_livo.log 2>&1 &
GZPID=$!
for i in $(seq 1 15); do
    kill -0 $GZPID 2>/dev/null || { echo "gzserver died"; exit 1; }
    timeout 2 gz topic -l 2>/dev/null | grep -q world_stats && break
    sleep 1
done
echo "gzserver ready"

echo "=== 2. Spawning iris_stereo_velodyne ==="
gz model --spawn-file="$SDF" --model-name=iris -x 0 -y 0 -z 0.83 2>&1 | head -3
sleep 3

echo "=== 3. Starting PX4 ==="
cd $PX4_DIR
$BUILD_DIR/bin/px4 -i 0 > /tmp/px4_livo.log 2>&1 &
PX4PID=$!
cd /home/travis/zcw/1.2
sleep 5
kill -0 $PX4PID 2>/dev/null || { echo "PX4 failed to start"; exit 1; }
echo "PX4 running"

echo "=== 4. Starting MAVROS (direct node) ==="
ros2 run mavros mavros_node --ros-args \
  -p fcu_url:=udp://:14540@127.0.0.1:14580 \
  -p system_id:=1 -p component_id:=1 -p use_sim_time:=True \
  > /tmp/mavros_livo.log 2>&1 &
MAVROSPID=$!
sleep 5

echo "=== 5. Starting FAST-LIVO2 ==="
LD_LIBRARY_PATH="/home/travis/zcw/1.2/ros2_ws/install/vikit_common/lib:/home/travis/zcw/1.2/ros2_ws/install/vikit_ros/lib:$LD_LIBRARY_PATH" \
    /home/travis/zcw/1.2/ros2_ws/build/fast_livo2/fast_livo2 --ros-args \
    -p use_sim_time:=True \
    --params-file /home/travis/zcw/1.2/ros2_ws/src/fast_livo2/config/livo_config.yaml \
    > /tmp/livo_runtime.log 2>&1 &
LIVOPID=$!
sleep 10
kill -0 $LIVOPID 2>/dev/null && echo "FAST-LIVO2 RUNNING" || { echo "FAST-LIVO2 failed"; cat /tmp/livo_runtime.log | tail -10; exit 1; }

echo ""
echo "=== FAST-LIVO2 全栈运行中 ==="
echo "FAST-LIVO2 PID: $LIVOPID"
echo "输出话题: $(timeout 3 ros2 topic list 2>/dev/null | grep -c -E 'aft_mapped|cloud|Laser|path|imu_propagate') 个定位话题"
timeout 3 ros2 topic list 2>/dev/null | grep -E "aft_mapped|cloud|Laser|path|imu_propagate" | head -10
echo "=========================="
echo "按 Ctrl+C 停止所有进程"

wait
