# ZCW 过程日志

## 2026-06-05 — 仓库初始化

- 完成工作流文档 `docs/00_workflow.md`。
- 创建项目管理文件（`.gitignore`, `RUNBOOK.md`, `OPEN_SOURCE_AUDIT.md`）。
- 项目根目录 `1.2`。
- Git 仓库初始化（分支 `zcw1.2`），remote 指向 `https://github.com/chuanweizhang560-rgb/zcw1.2.git`。
- base/ 目录只保留项目管理文件：README.md、PROCESS_LOG.md、RUNBOOK.md、OPEN_SOURCE_AUDIT.md。
- 当前状态：阶段 0（仓库骨架）已完成。

## 2026-06-05 — 阶段 1：资产审计完成

- 环境确认：ROS2 Humble ✅、Gazebo 11.10.2 ✅、PX4 ❌待安装
- 风机模型审计完成 → 推荐第一版用过程式 SDF
- 输电杆塔审计完成 → 首选 guiaugustoga987/TL
- 导线/电缆方案审计完成 → TL静态导线(ROS1) / mmWave_ROS2(ROS2)
- 沙漠地形待补充调研
- 详细审计记录见 `OPEN_SOURCE_AUDIT.md`
- 下一步：等待确认方案方向后进入阶段 2

## 2026-06-05 — 阶段 2：仿真环境最小闭环

### 已完成
- PX4-Autopilot v1.14 克隆 + 编译 ✅（gazebo-classic, iris 模型）
- ROS 2 Humble workspace 构建 ✅（px4_msgs, px4_ros_com, zcw_offboard）
- MAVROS 安装（`ros-humble-mavros`）✅
- 资产 TL 模型已克隆到 `assets/models/TL`
- Offboard 控制节点 `zcw_offboard` ✅

### 闭环验证 ✅
```
PX4 SITL → UDP 14580 → MAVROS → ROS 2 → offboard_control
```
- [x] MAVROS 与 PX4 握手
- [x] 无人机解锁（ARM）
- [x] 切换 OFFBOARD 模式
- [x] 发送位置 setpoint 到 10m

### 启动方法
```bash
# 终端 1: PX4 + Gazebo
cd PX4-Autopilot && make px4_sitl gazebo-classic_iris

# 终端 2: MAVROS + Offboard
source /opt/ros/humble/setup.bash
source ros2_ws/install/local_setup.bash
ros2 launch zcw_offboard test_minimal.launch.py
```

### 待做
- [ ] 阶段 3：接入 TL 杆塔模型到 Gazebo 世界
- [ ] 沙漠地形调研（阶段 1 遗留）

---

_日志格式：YYYY-MM-DD — 事件描述_
