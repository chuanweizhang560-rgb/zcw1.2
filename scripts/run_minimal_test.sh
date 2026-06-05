#!/bin/bash
# ZCW Phase 2.6 — PX4 SITL + Gazebo + MAVROS + Offboard 最小闭环测试
# 在桌面终端直接运行: ./scripts/run_minimal_test.sh

PROJ_DIR=/home/travis/zcw/1.2

cleanup() {
    echo ""
    echo "=== 清理 ==="
    kill 0 2>/dev/null || true
    sleep 1
    killall -9 gzserver gzclient px4 2>/dev/null || true
    echo "已停止"
}
trap cleanup EXIT

source /opt/ros/humble/setup.bash
source ${PROJ_DIR}/ros2_ws/install/local_setup.bash

export PX4_ROOT=${PROJ_DIR}/PX4-Autopilot
export GAZEBO_MODEL_PATH=${PX4_ROOT}/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models:${PROJ_DIR}/assets/models
export GAZEBO_PLUGIN_PATH=${PX4_ROOT}/build/px4_sitl_default/build_gazebo-classic
export LD_LIBRARY_PATH=${GAZEBO_PLUGIN_PATH}:${LD_LIBRARY_PATH}

echo "============================================"
echo "  ZCW 最小闭环测试"
echo "  PX4 SITL + Gazebo + MAVROS + Offboard"
echo "============================================"

echo ""
echo "[1/3] 启动 PX4 SITL + Gazebo (Iris)..."
cd ${PX4_ROOT}
make px4_sitl gazebo-classic_iris &>/tmp/zcw_px4.log &
sleep 15

echo "[2/3] 启动 MAVROS..."
ros2 run mavros mavros_node --ros-args \
  -p fcu_url:=udp://:14540@127.0.0.1:14580 \
  -p system_id:=1 -p component_id:=1 &>/tmp/zcw_mavros.log &
sleep 8

echo "[3/3] 启动 Offboard 控制 (起飞到 10m)..."
ros2 run zcw_offboard offboard_control &>/tmp/zcw_offboard.log &

echo ""
echo "=== 全部已启动 ==="
echo "  日志: tail -f /tmp/zcw_px4.log"
echo "  日志: tail -f /tmp/zcw_mavros.log"
echo "  日志: tail -f /tmp/zcw_offboard.log"
echo ""
echo "Gazebo GUI 应已显示 Iris 无人机"
echo "按 Enter 停止所有进程"
read -r
