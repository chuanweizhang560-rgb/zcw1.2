#!/bin/bash
# ZCW 1.2 项目环境配置
# source 此文件而非 .bashrc 中的 BS setup，避免库冲突
#
# 用法:
#   source /path/to/zcw1.2/setup.bash
# 或放在 ~/.bashrc 按项目切换时选择 source

# 记录当前路径
ZCW_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 1. 剔除 BS 项目路径（避免库冲突）
__clean_path() {
    local var="$1"
    local val="${!var}"
    local new_val=""
    IFS=':' read -ra parts <<< "$val"
    for part in "${parts[@]}"; do
        case "$part" in
            */BS/*) ;;
            *) new_val="${new_val:+${new_val}:}${part}" ;;
        esac
    done
    export "$var"="$new_val"
}

__clean_path LD_LIBRARY_PATH
__clean_path GAZEBO_MODEL_PATH
__clean_path GAZEBO_PLUGIN_PATH
__clean_path AMENT_PREFIX_PATH
__clean_path PYTHONPATH
__clean_path CMAKE_PREFIX_PATH

# 2. Source ROS 2 + Gazebo
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh

# 3. Source PX4 gazebo 环境
source "${ZCW_ROOT}/PX4-Autopilot/Tools/simulation/gazebo-classic/setup_gazebo.bash" \
    "${ZCW_ROOT}/PX4-Autopilot" \
    "${ZCW_ROOT}/PX4-Autopilot/build/px4_sitl_default"

# 4. Source ROS 2 workspace
if [ -f "${ZCW_ROOT}/ros2_ws/install/local_setup.bash" ]; then
    source "${ZCW_ROOT}/ros2_ws/install/local_setup.bash"
fi

# 5. 项目别名
alias zcw-px4='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=tower HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-px4-turbine='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=turbine_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-gzclient='gzclient --verbose &'
alias zcw-offboard='ros2 launch zcw_offboard test_minimal.launch.py'

echo "ZCW 1.2 环境就绪 (RTX 4060 GPU 渲染)"
echo "  快捷命令:"
echo "    zcw-px4      - 启动 PX4 SITL (tower 场景)"
echo "    zcw-gzclient - 启动 Gazebo GUI"
echo "    zcw-offboard - 启动 offboard 控制"
