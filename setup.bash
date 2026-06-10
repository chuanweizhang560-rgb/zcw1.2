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

# 1b. Add conda lib path only for commands that need it later (not globally, to avoid hanging system tools)
# FAST-LIVO2 alias will handle this internally
export PATH="/home/travis/miniconda3/bin:$PATH"

# 2. Source ROS 2 + Gazebo
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh

# 3. Source PX4 gazebo 环境
source "${ZCW_ROOT}/PX4-Autopilot/Tools/simulation/gazebo-classic/setup_gazebo.bash" \
    "${ZCW_ROOT}/PX4-Autopilot" \
    "${ZCW_ROOT}/PX4-Autopilot/build/px4_sitl_default"

# 4. 添加项目自定义模型路径
export GAZEBO_MODEL_PATH="${ZCW_ROOT}/assets/models:${GAZEBO_MODEL_PATH}"

# 5. Source ROS 2 workspace
if [ -f "${ZCW_ROOT}/ros2_ws/install/local_setup.bash" ]; then
    source "${ZCW_ROOT}/ros2_ws/install/local_setup.bash"
fi

# 5. 项目别名
alias zcw-px4='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=tower HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-px4-turbine='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=turbine_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-px4-cable='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=cable_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-px4-cable-desert='cd ${ZCW_ROOT}/PX4-Autopilot && PX4_SITL_WORLD=cable_inspection_desert HEADLESS=1 make px4_sitl gazebo-classic_iris'
alias zcw-gzclient='gzclient --verbose &'
alias zcw-gzclient-follow='gzclient --verbose --gui-client-plugin libgazebo_user_camera_plugin.so &'
alias zcw-offboard='ros2 launch zcw_offboard test_minimal.launch.py'
alias zcw-inspect='ros2 run zcw_offboard inspection_control --ros-args -p radius:=80.0 -p height:=30.0 -p angular_velocity:=0.1 -p center_x:=80.0 -p center_y:=0.0'
alias zcw-cable-follow='ros2 run zcw_offboard cable_follow_control --ros-args -p tower_x1:=-30.0 -p tower_x2:=30.0 -p cable_y:=0.6'
alias zcw-cable-track='ros2 run zcw_offboard cable_tracker --ros-args -p tower_x1:=-30.0 -p tower_x2:=30.0'
alias zcw-inspect-dual='ros2 run zcw_offboard cable_tracker --ros-args -p mavros_ns:=uav1 -p tower_x1:=-30.0 -p tower_x2:=30.0'
alias zcw-relay='ros2 run zcw_offboard relay_control --ros-args -p own_ns:=uav2 -p inspect_ns:=uav1'
alias zcw-dual='bash ${ZCW_ROOT}/scripts/start_dual_sim.sh'
alias zcw-4uav='bash ${ZCW_ROOT}/scripts/start_4uav_sim.sh'
alias zcw-mavros='ros2 launch mavros px4.launch fcu_url:="udp://:14540@127.0.0.1:14540" namespace:=uav1 tgt_system:=1 tgt_component:=1'
alias zcw-role='ros2 run zcw_offboard role_allocator --ros-args -p num_uavs:=4'
alias zcw-explore='ros2 run zcw_offboard explore_control --ros-args -p own_ns:=uav3 -p scout_dist:=20.0 -p altitude:=20.0'
alias zcw-reserve='ros2 run zcw_offboard reserve_control --ros-args -p own_ns:=uav4 -p hold_x:=0.0 -p hold_y:=20.0 -p hold_z:=20.0'
alias zcw-track-rl='PYTHONPATH=${ZCW_ROOT}/zcw_rl/src:$PYTHONPATH /usr/bin/python3 ${ZCW_ROOT}/ros2_ws/src/zcw_offboard/scripts/cable_tracker_rl.py --ros-args -p tower_x1:=-30.0 -p tower_x2:=30.0 -p model_path:=${ZCW_ROOT}/zcw_rl/models/ppo_cable_tracker_v4_final.zip'

echo "ZCW 1.2 环境就绪 (RTX 4060 GPU 渲染)"
echo "  快捷命令:"
echo "    zcw-px4          - 启动 PX4 SITL (tower 场景)"
echo "    zcw-px4-turbine  - 启动 PX4 SITL (风机场景)"
echo "    zcw-px4-cable    - 启动 PX4 SITL (电缆场景)"
echo "    zcw-gzclient     - 启动 Gazebo GUI"
echo "    zcw-gzclient-follow - 启动 Gazebo GUI (自动追踪)"
echo "    zcw-inspect      - 启动风机环绕巡检控制器"
echo "    zcw-cable-follow - 启动电缆飞线控制器"
echo "    zcw-cable-track  - 启动电缆跟踪器 (Frenet+重捕获)"
echo "    zcw-dual         - 启动双机仿真(INSPECT+RELAY)"
echo "    zcw-4uav         - 启动四机仿真(全角色)"
echo "    zcw-mavros      - 启动 MAVROS (uav1, 单机模式)"
echo "    zcw-inspect-dual - 双机: 巡检控制器"
echo "    zcw-relay        - 双机: 中继控制器"
echo "    zcw-role        - 角色分配器"
echo "    zcw-explore     - 探索控制器"
echo "    zcw-reserve     - 待命控制器"
echo "    zcw-track-rl    - RL 电缆跟踪 (PPO)"
echo "    zcw-livo        - FAST-LIVO2 单机启动（LiDAR+IMU，需已有数据源）
    zcw-livo-full   - FAST-LIVO2 全栈验证（Gazebo+PX4+MAVROS+FAST-LIVO2）"

# FAST-LIVO2 aliases
alias zcw-livo="LD_LIBRARY_PATH=\"/home/travis/zcw/1.2/ros2_ws/install/vikit_common/lib:/home/travis/zcw/1.2/ros2_ws/install/vikit_ros/lib:\$LD_LIBRARY_PATH\" /home/travis/zcw/1.2/ros2_ws/build/fast_livo2/fast_livo2 --ros-args -p use_sim_time:=True --params-file /home/travis/zcw/1.2/ros2_ws/src/fast_livo2/config/livo_config.yaml"

alias zcw-livo-full='bash /home/travis/zcw/1.2/scripts/start_livo_full.sh'
