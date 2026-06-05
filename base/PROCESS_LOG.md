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

### 已知问题
- Gazebo GUI (gzclient) 需在 `source /usr/share/gazebo/setup.sh` 后独立启动
- PX4 用 `HEADLESS=1 make` 只起 gzserver，然后手动 `gzclient --verbose &`
- BS 项目 ROS 库会污染 LD_LIBRARY_PATH，导致 GPU 渲染崩溃（黑屏闪退）
- 每次启动必须 source setup.bash 剔除 BS 路径后方可 GPU 渲染
- 详细说明见 `docs/gazebo_gui_notes.md`

### 提交
- 阶段 2 首次提交 ✅（含闭环验证通过的完整代码和资产）

### 阶段 3：TL 杆塔模型接入

2026-06-05

### 已完成
- TL 模型（guiaugustoga987/TL）复制到 PX4 模型路径 ✅
- 创建自定义世界 `assets/worlds/tower.world` ✅
  - 含两个杆塔：tower1 @ (30,0), tower2 @ (-30, 0)
  - 每基杆塔 21 个 STL 网格（塔身 + 6 根导线 + 支撑结构）
- TL 模型设为 static（避免物理引擎负载过高导致渲染卡死）
- 通过 `PX4_SITL_WORLD=tower` 环境变量切换世界

### 验证 ✅
- gzserver + tower.world 直接启动 → Gazebo GUI 显示杆塔 ✅
- PX4 SITL + tower.world + gzclient 全栈启动 → 窗口可见 ✅
- 已确认用户能同时看到 Iris 无人机和两基杆塔

### 启动方法（带杆塔场景）
```bash
cd PX4-Autopilot
PX4_SITL_WORLD=tower HEADLESS=1 make px4_sitl gazebo-classic_iris

# 另一终端
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
gzclient --verbose
```

### 已知问题
- Gazebo GUI (gzclient) 需 `LIBGL_ALWAYS_SOFTWARE=1` 才能稳定运行（软件渲染）
- TL 模型若设非 static，物理引擎会因 42 个碰撞网格（21/塔 × 2 塔）而卡死
- 详细记录见 `docs/gazebo_gui_notes.md`

### 关键发现
- Gazebo GPU 黑屏闪退根因：BS 项目 (`~/.bashrc:149`) 的 ROS 库优先级高于系统库，导致 OGRE 加载冲突 `.so` 崩溃
- 修复：创建 `setup.bash` 剔除 BS 路径，使用 NVIDIA RTX 4060 硬件渲染
- TL 模型若设非 static，42 个碰撞网格拖垮 ODE 物理引擎

### 启动流程（修正后）
```bash
# 终端 1: 先 source 项目专用环境
source /home/travis/zcw/1.2/setup.bash
# 然后启动 PX4
zcw-px4    # alias for: cd PX4-Autopilot && PX4_SITL_WORLD=tower HEADLESS=1 make px4_sitl gazebo-classic_iris

# 终端 2: 同样 source 环境，启动 GUI
source /home/travis/zcw/1.2/setup.bash
zcw-gzclient    # alias for: gzclient --verbose &
zcw-offboard    # alias for: ros2 launch zcw_offboard test_minimal.launch.py
```

### 待做
- [ ] 沙漠地形调研（阶段 1 遗留）
- [ ] 多无人机协同（后续阶段）
- [ ] 电缆跟踪视觉（后续阶段）

---

### 启动方法（原始 empty.world）
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
