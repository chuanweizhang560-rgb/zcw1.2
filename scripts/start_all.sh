#!/usr/bin/env bash
set -e

source /opt/ros/humble/setup.bash 2>/dev/null
source /usr/share/gazebo/setup.sh 2>/dev/null
source /home/travis/zcw/1.2/setup.bash 2>/dev/null

# PX4 paths
PX4_BIN=/home/travis/zcw/1.2/PX4-Autopilot/build/px4_sitl_default/bin/px4
BUILD_DIR=/home/travis/zcw/1.2/PX4-Autopilot/build/px4_sitl_default
ROOTFS=$BUILD_DIR/rootfs
SRC=/home/travis/zcw/1.2/PX4-Autopilot

# 1. Source PX4 gazebo env
source "$SRC/Tools/simulation/gazebo-classic/setup_gazebo.bash" "$SRC" "$BUILD_DIR"
export GAZEBO_MODEL_DATABASE_URI=""
export GAZEBO_MODEL_PATH="$SRC/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models:$GAZEBO_MODEL_PATH"

pkill -9 -f "gzserver|gzclient|px4|mavros_node" 2>/dev/null
sleep 2

# 2. Start gzserver with cable world
echo ">> Starting gzserver..."
setsid gzserver /home/travis/zcw/1.2/assets/worlds/cable_inspection.world --verbose \
  -s libgazebo_ros_init.so -s libgazebo_ros_factory.so < /dev/null > /dev/null 2>&1 &
sleep 4

# 3. Spawn iris
echo ">> Spawning iris..."
/usr/bin/python3 /opt/ros/humble/lib/gazebo_ros/spawn_entity.py \
  -file "$SRC/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris/iris.sdf" \
  -entity iris -x 0 -y 0 -z 0.83 > /dev/null 2>&1
sleep 3

# 4. PX4 启动指令 (终端里单独运行):
echo ""
echo "============================================"
echo ">> gzserver + iris 已就绪"
echo ">> 新开一个终端运行 PX4:"
echo ""
echo "source ~/zcw/1.2/setup.bash"
echo "cd ~/zcw/1.2/PX4-Autopilot/build/px4_sitl_default/rootfs && rm -rf 0"
echo "../../bin/px4 -d ../etc"
echo "============================================"
