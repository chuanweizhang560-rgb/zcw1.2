#!/bin/bash
# Phase 8.2: FAST-LIVO2 全栈验证脚本（单模型方案）
# gzserver + iris_stereo_velodyne(传感器+飞行器) + PX4 + MAVROS + FAST-LIVO2
set -e
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
source /home/travis/zcw/1.2/setup.bash
export LD_LIBRARY_PATH="/opt/ros/humble/lib:$LD_LIBRARY_PATH:/home/travis/miniconda3/lib"

PX4_DIR=/home/travis/zcw/1.2/PX4-Autopilot
BUILD_DIR=$PX4_DIR/build/px4_sitl_default
WORLD=/home/travis/zcw/1.2/assets/worlds/cable_inspection.world
CUSTOM_SDF=$PX4_DIR/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris_stereo_velodyne/model.sdf
export PX4_LOCKSTEP=0
export PX4_SIM_MODEL=gazebo-classic_iris

cleanup() {
    echo "=== CLEANUP ==="
    pkill -f fast_livo2 2>/dev/null || true
    pkill -f vision_pose_relay 2>/dev/null || true
    pkill -f lidar_relay 2>/dev/null || true
    pkill -f mavros_node 2>/dev/null || true
    pkill -f px4 2>/dev/null || true
    pkill -f gzserver 2>/dev/null || true
    pkill -f gzclient 2>/dev/null || true
    sleep 2; echo "done"
}
trap cleanup EXIT

echo "=== 1. Clean PX4 params ==="
rm -f "$BUILD_DIR/rootfs/0/parameters.bson" "$BUILD_DIR/rootfs/0/parameters_backup.bson"
PARAM_FILE="$BUILD_DIR/rootfs/0/etc/init.d-posix/px4-rc.params"
mkdir -p "$(dirname $PARAM_FILE)"
cp /home/travis/zcw/1.2/PX4-Autopilot/ROMFS/px4fmu_common/init.d-posix/px4-rc.params "$PARAM_FILE"
cat >> "$PARAM_FILE" << 'PEOF'
param set EKF2_MULTI_IMU 1
PEOF

echo "=== 2. gzserver ==="
killall -9 gzserver gzclient 2>/dev/null || true
gzserver "$WORLD" --verbose > /tmp/gz_livo.log 2>&1 &
sleep 10

echo "=== 3. Spawn iris_stereo_velodyne ==="
gz model --spawn-file="$CUSTOM_SDF" --model-name=iris -x 1 -y 0 -z 0.83 2>/dev/null
sleep 15

echo "=== 4. PX4 ==="
cd $PX4_DIR
$BUILD_DIR/bin/px4 -d -i 0 > /tmp/px4_livo.log 2>&1 &
cd /home/travis/zcw/1.2
for i in $(seq 1 60); do
    grep -q "Ready for takeoff" /tmp/px4_livo.log 2>/dev/null && { echo "✅ PX4 at ${i}s"; break; }
    sleep 1
done

echo "=== 5. MAVROS ==="
ros2 run mavros mavros_node --ros-args \
  -p fcu_url:=udp://:14540@127.0.0.1:14580 \
  -p system_id:=1 -p component_id:=1 -p use_sim_time:=True \
  > /tmp/mavros_livo.log 2>&1 &
sleep 10

echo "=== 6. LiDAR relay (VLP-16 field fix) ==="
/usr/bin/python3 /home/travis/zcw/1.2/ros2_ws/src/zcw_offboard/scripts/lidar_relay.py > /tmp/relay_livo.log 2>&1 &
sleep 3

echo "=== 7. Vision pose relay (FAST-LIVO2 → /mavros/vision_pose/pose) ==="
/usr/bin/python3 /home/travis/zcw/1.2/ros2_ws/src/zcw_offboard/scripts/vision_pose_relay.py > /tmp/vp_relay.log 2>&1 &
sleep 3

echo "=== 8. FAST-LIVO2 (LIO mode) ==="
LIVO_BIN=/home/travis/zcw/1.2/ros2_ws/install/fast_livo2/lib/fast_livo2/fast_livo2
$LIVO_BIN --ros-args -p use_sim_time:=True \
    --params-file /home/travis/zcw/1.2/ros2_ws/src/fast_livo2/config/livo_config.yaml \
    > /tmp/livo_runtime.log 2>&1 &
sleep 10

echo "=== FAST-LIVO2 全栈运行中 ==="
echo "LiDAR: /velodyne_laser_plugin/out → relay → /velodyne/points_raw"
echo "IMU:   /mavros/imu/data (PX4→MAVROS)"
echo "SLAM:  /aft_mapped_to_init → vision_pose_relay → /mavros/vision_pose/pose"
echo "Press Ctrl+C to stop"
wait
