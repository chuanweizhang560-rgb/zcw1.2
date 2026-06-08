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

## 2026-06-05 — 阶段 3b：风机模型接入

### 已完成
- VIS4ROB 70m 风机模型（SDF+DAE）克隆到 `assets/models/wind_turbine_70m/`
- 模型为 static（无物理碰撞，仅视觉）
- 创建 `assets/worlds/turbine_inspection.world`（含风机 + 地面 + 光照）
- 已同步到 PX4 模型路径和 world 路径
- GPU 渲染下验证通过，用户确认可见

### 启动
```bash
PX4_SITL_WORLD=turbine_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris
```

---

### 阶段 3a：TL 杆塔模型接入

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

## 2026-06-05 — 阶段 3c：单机风机环绕巡检

### 已完成
- 编写 `inspection_control.cpp` — 圆形环绕轨迹控制器
  - 可配置半径、高度、角速度、环绕中心
  - 自动 ARM → OFFBOARD → 环绕序列
  - Yaw 始终朝向风机中心 `(cx, cy)`
- 创建 `launch/inspection.launch.py`（含 MAVROS）
- 修改 Iris 颜色为 `Gazebo/White`（白色，更醒目）
- 世界文件增加 `<gui><camera>` 初始视角配置
- 验证：无人机 ARM → 起飞 → 环绕风机飞行 ✅
  - 高度 30m，半径 80m（围绕 (80,0)）
  - GPS 渲染下 gzclient 稳定运行
  - 自动追踪相机插件可用：`--gui-client-plugin libgazebo_user_camera_plugin.so`

### 问题记录
- 旧 `offboard_control` 进程残留在后台，与新 `inspection_control` 争夺 setpoint 发布，导致无人机悬停不动
  - 修复：启动前 `killall -9 inspection_control offboard_control` 清理残存进程
- 控制器进程因 bash 工具超时而非崩溃退出，无 error log
  - 修复：`nohup` 启动后 `disown` 保持后台运行
- Gazebo 软件渲染（`LIBGL_ALWAYS_SOFTWARE=1`）更稳定

### 启动
```bash
source /home/travis/zcw/1.2/setup.bash
# 终端 1
cd PX4-Autopilot
PX4_SITL_WORLD=turbine_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris
# 终端 2
source /usr/share/gazebo/setup.sh
LIBGL_ALWAYS_SOFTWARE=1 gzclient --verbose --gui-client-plugin libgazebo_user_camera_plugin.so
# 终端 3
source ros2_ws/install/local_setup.bash
ros2 launch mavros px4.launch fcu_url:="udp://:14540@127.0.0.1:14580"
# 终端 4
source ros2_ws/install/local_setup.bash
ros2 run zcw_offboard inspection_control \
  --ros-args -p radius:=80.0 -p height:=30.0 -p angular_velocity:=0.1 \
  -p center_x:=80.0 -p center_y:=0.0
```

## 2026-06-05 — 阶段 4.1：单机电缆巡检 — 导线中心线先验飞行

### 已完成
- 创建 `cable_inspection.world` — 两基 TL 杆塔（-30m / +30m），使用模型自带的6根导线
- 编写 `cable_follow_control.cpp` — 航点跟随飞线控制器
  - 抛物线近似 catenary：附着点 25m，垂度 4m
  - 20 个航点从 tower2 飞到 tower1
  - 自适应 QoS（BEST_EFFORT）兼容 MAVROS 位置话题
- 验证：从 (-24, 0.6, 23.5) → (22.6, 0.5, 21.6) 沿导线路径飞行成功 ✅
- setup.bash 添加 `zcw-px4-cable` / `zcw-cable-follow` 别名

### 问题记录
- MAVROS 发布 `/mavros/local_position/pose` 使用 BEST_EFFORT QoS，订阅默认 RELIABLE 不兼容 → `dist_to_wp()` 永远返回大值，控制器不前进
  - 修复：`rclcpp::QoS(rclcpp::KeepLast(10)).best_effort()`
- 最初在 world 中添加了分段圆柱模拟导线，但 TL 模型自身已有导线；且圆柱因未先旋转 -π/2 放平而是竖在空中。已全部移除，只保留两基 TL 塔。

### 启动
```bash
source /home/travis/zcw/1.2/setup.bash
# 终端 1: PX4 SITL
PX4_SITL_WORLD=cable_inspection HEADLESS=1 make px4_sitl gazebo-classic_iris
# 终端 2: Gazebo GUI
LIBGL_ALWAYS_SOFTWARE=1 gzclient --verbose --gui-client-plugin libgazebo_user_camera_plugin.so
# 终端 3: MAVROS
ros2 launch mavros px4.launch fcu_url:="udp://:14540@127.0.0.1:14580"
# 终端 4: 飞线控制
ros2 run zcw_offboard cable_follow_control \
  --ros-args -p tower_x1:=-30.0 -p tower_x2:=30.0 -p cable_y:=0.6
```

### 待做
- [x] 阶段 4.2：点云线拟合离线验证
- [ ] 阶段 4.3：在线中心线跟随（含重捕获）
- [ ] 沙漠地形调研（阶段 1 遗留）

---

## 2026-06-05 — 阶段 4.2：点云线拟合离线验证

### 已完成
- 编写 `cable_fit_offline.cpp` — 基于 PCL RANSAC 的导线拟合离线验证工具
  - 生成已知 catenary 导线点云（可调噪声水平）
  - RANSAC SACMODEL_LINE 拟合 + 真实方向对比
  - 噪声敏感度分析（σ=0.02~0.2m）
- 编译为独立可执行文件 `scripts/cable_fit_offline`

### 验证结论
- **直线模型(LINE)不适用于弧线导线**：整条导线仅 33% 内点率
- **短弧段可行**：20m 段 σ=0.1m 时 RMSE=0.18m、方向角差 3.2°
- **需要 catenary/spline 曲线模型**才能做全长导线跟踪
- 符合工作流设计：Phase 4.3 须用 Frenet 框架做曲线跟随

### 启动
```bash
./scripts/cable_fit_offline
```

### 待做
- [ ] 阶段 4.3：在线中心线跟随（含重捕获）
- [ ] 沙漠地形调研（阶段 1 遗留）

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
