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

## 2026-06-05 — 阶段 4.4：失锁回退

### 已完成
- 增强 cable_tracker v2 — 多级失锁回退状态机
  - **SWEEP**：正弦横摆搜索模式，振幅逐次增大 2→4→6m
  - **FALLBACK**：step-down 返回原点 15m→10m→5m→2m
  - **LAND**：落地后自动 disarm
  - 每次状态切换记录详细信息：t、err、pos
- 正常跟踪已验证（t=0→1.00, err<0.6m）

### 启动
```bash
source setup.bash && source ros2_ws/install/local_setup.bash
zcw-px4-cable        # 终端 1
zcw-gzclient         # 终端 2
zcw-mavros           # 终端 3
zcw-cable-track      # 终端 4 (v2 fallback)
```

### 待做
- [ ] 沙漠地形调研（阶段 1 遗留）

---

## 2026-06-05 — 阶段 4.3：在线中心线跟随（含重捕获）

### 已完成
- 编写 `cable_tracker.cpp` — Frenet 框架在线导线跟随控制器
  - **状态机**：APPROACH → DESCEND → TRACK → RECAPTURE → FALLBACK → DONE
  - **Frenet 跟随**：单调前进 t_progress，始终向前沿导线推进
  - **重捕获**：误差 >5m 持续 >5s → 飞往上次良好位置 + 偏置搜索
  - **失锁回退**：连续 3 次重捕获失败 → 返回原点
  - **端到端清理**：到达终点后自动 disarm
  - 每 5s 定期输出进度：t_progress、位置、误差
- setup.bash 添加 `zcw-cable-track` 快捷命令
- 实测验证：t=0.00→0.90 沿 Cable 4 从 tower2 飞行至 tower1，误差 <0.6m ✅

### 验证结果
```
TRACK t=0.00 pos=(-29.7,0.7,26.7) err=1.76
TRACK t=0.45 pos=(-9.1,0.6,21.4) err=0.53
TRACK t=0.90 pos=(17.9,0.6,22.6) err=0.15
TRACK complete! Arrived at tower1. Disarming...
```

### 启动
```bash
source setup.bash && source ros2_ws/install/local_setup.bash
zcw-px4-cable        # 终端 1: PX4 + Gazebo
zcw-gzclient         # 终端 2: GUI
zcw-mavros           # 终端 3: MAVROS
zcw-cable-track      # 终端 4: Follow cable
```

### 待做
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

## 2026-06-09 — Phase 8: 沙漠场景完善

- 完成程序化沙漠世界生成器 `scripts/world_gen/`（YAML 配置 + Perlin 地形 + Jinja2 模板）
- 500×500m 地形：5 区域类型（basin/plain/dune/wadi/mesa），50m 起伏，沙色纹理
- 8 风机：随机布局 + 网格级缩放（Gazebo 11 兼容 inline model）
- 4 杆塔对：逐柱采样地形高度，Grey 材质
- 8 不规则岩石模型（icosphere 扰动网格）：60 块散布，0.2~4.7 scale，暗褐色
- 80 植被：棕榈树 + 灌木，Z偏移-1m 防悬空
- 基站平台 + Iris 无人机预览
- 关键 Bug 记录：Gazebo 11 忽略 `<include><scale>`，须用 inline `<mesh><scale>`
- 提交: 33c327e (已推送)

## 2026-06-09 — 全栈集成测试（沙漠 seed 42）

- 在 seed 42 沙漠世界（中心海拔 ~42m 高地）完成 4 机全栈测试
- 启动流程：gzserver → spawn 4 Iris → 4×PX4(LOCKSTEP=0) → 4×MAVROS → controllers
- 测试结果：
  - uav1(EXPLORE): CONN OFFBOARD ARM ✅ 探索航点
  - uav2(RELAY): CONN OFFBOARD ARM ✅ 跟随 uav1
  - uav3(EXPLORE): CONN OFFBOARD ARM ✅ 远距扫描
  - uav4(RESERVE): CONN OFFBOARD ARM ✅ 待命保持
- 关键参数：PX4_LOCKSTEP=0, spawn z=42.5（地形高度匹配）
- 脚本: scripts/start_4uav_desert.sh

### 待做
- [ ] SLAM 方案调研与集成 ⬅️ 当前阶段 (Phase 8)
- [ ] 增量建图 + 多机地图共享 (Phase 9)
- [ ] SLAM + 建图 + 任务 + MAPPO 完整链路集成 (Phase 10)

---

## 2026-06-09 — 工作流更新：SLAM + 建图 + 多编队架构

- 更新 docs/00_workflow.md 以反映真实项目架构
- 新增 Section 4：SLAM 与建图架构（ORB-SLAM3 / VINS-Fusion / DroidSLAM 候选）
- 新增 Section 4.4：SLAM 与控制耦合（基于 SLAM 位姿，断开 Gazebo 真值）
- 新增 Section 8.3：全局地图共享机制
- 新增 Section 8.4：多编队扩展规划（1 编队×4 机 → 多编队）
- 新增 Section 9.3：RL 观测空间（含地图输入）
- 新增 Section 9.6：训练阶段 5 步计划
- 新增 Phase 8-10：SLAM 调研 → 增量建图 → 全链路集成
- 更新 Section 0：新增规则 12-13（不依赖真值、核心算法必须用开源）
- 更新 Section 12：新增 SLAM 选型、地图表示、编队扩展待确认项
- 关键约束已写入持久记忆
- 提交: 7ab0530 (已推送)

## 2026-06-10 — 阶段 8：FAST-LIVO2 ROS 2 适配 + 全栈验证 ✅

### 适配修复（在首次编译基础上）
- **LIO-only 模式修复**:
  - `LIVMapper.h: img_en` 成员初始化 `1→0`，确保默认关闭相机
  - `LIVMapper.cpp`: `initializeVIO()` 等相机初始化移入 `if (img_en) {}` 块，防止空指针访问
  - `livo_config.yaml`: 层级结构修复——参数路径用 `common.*`/`vio.*`/ 前缀匹配 `GET_PARAM` 命名
- **已验证**：FAST-LIVO2 在 LIO-only 模式下发布 `/aft_mapped_to_init`、`/cloud_registered`、`/Laser_map`、`/path` 等话题

### 全栈验证
- **Gazebo 模型**: `iris_stereo_velodyne`（VLP-16 LiDAR + 双目 + IMU）
- **启动流程**:
  1. `gzserver` + `cable_inspection.world`（含 TL 杆塔）
  2. `gz model --spawn-file` 载入 `iris_stereo_velodyne`
  3. PX4 SITL（`-i 0`）+ MAVROS（`/uav1/imu/data`）
  4. FAST-LIVO2 订阅 `/velodyne_laser_plugin/out` + `/uav1/imu/data`
- **验证结果**: FAST-LIVO2 ✅ 正常运行（输出定位话题）
- **脚本**: `scripts/start_livo_full.sh` — 一键启动全栈验证
- **快捷命令**: `zcw-livo`（单机启动）+ `zcw-livo-full`（全栈脚本）
- **提交**: ec17141

### IMU 数据流排查 ✅
- **根因 1**: `ros2 launch mavros px4.launch` 传递参数方式与 `ros2 run mavros mavros_node --ros-args` 不同，前者通过 launch file 转发的参数导致 IMU 数据不发布
- **根因 2**: FAST-LIVO2 初始化 segfault — `extrin_calib.extrinsic_T` 和 `extrinsic_R` 缺省为空向量，`initializeComponents()` 中 `VEC_FROM_ARRAY` 访问越界
- **修复**: 
  - MAVROS 用 `ros2 run mavros mavros_node` 直接启动，参数 `fcu_url:=udp://:14540@127.0.0.1:14580 system_id:=1 component_id:=1 use_sim_time:=True`
  - FAST-LIVO2 配置添加 `extrin_calib.extrinsic_T/R/Pcl/Rcl` 完整参数，`preprocess.lidar_type=2`(VELO16)，`preprocess.scan_line=16`
  - 两边都需要 `use_sim_time:=True` 保证 LiDAR+IMU 时间戳一致
- **全栈验证结果**: FAST-LIVO2 LIO 模式完整运行
  - LiDAR: 10Hz, 1641 raw features/frame, 29 downsampled, 0-2 effective
  - IMU: 50Hz via MAVROS
  - 输出: `/aft_mapped_to_init`(位姿) `/cloud_registered`(注册点云) `/Laser_map`(激光地图) `/path`(轨迹) `/cloud_effected`(有效特征) `/LIVO2/imu_propagate`(IMU传播)
- **livo_config.yaml** 更新: `imu_topic: "/mavros/imu/data"`

## 2026-06-11 — 阶段 8.2：FAST-LIVO2 → EKF2 全数据流验证 ✅

### 关键修复：LiDAR PointCloud2 字段类型
- Gazebo CPU ray VLP-16 输出的 PointCloud2 字段声明与实际数据布局不匹配
- **报错**: `Failed to find match for field 'time'` (PCL fromROSMsg 找不到 time 字段)
- **根因**: velodyne 插件声明 `time` 字段为 FLOAT32，但 PCL `velodyne_ros::Point` 需要精确对应
- **修复**: `lidar_relay.py` 重新打包 PointCloud2，保留 `time` 字段 (FLOAT32, offset=18)
- **结果**: `point_step=22`, len(`fields`)=6: `x,y,z,intensity,ring,time` — PCL 转换正常

### 关键发现：MAVROS + IMU 是 FAST-LIVO2 LIO 模式的前提
- **FAST-LIVO2 LIO 模式** (`slam_mode_=ONLY_LIO=1`): `sync_packages()` 需要 `imu_buffer` 不为空才能通过等待
- **FAST-LIVO2 LO 模式** (`slam_mode_=ONLY_LO=0`): `case ONLY_LO` 处理逻辑中不等待 IMU，但 `run()` 循环的 `processImu()` 调用在 `imu_en=false` 时静默跳过，不影响主流程
- **实际验证**: 仅有 LiDAR 时 FAST-LIVO2 `run()` 中 `sync_packages()` 永远返回 false，不进入处理循环
- **结论**: FAST-LIVO2 必须运行在 LIO 模式 (LiDAR + IMU)，pure LO 模式在代码中未正常工作

### 全数据流验证结果 ✅
```
LiDAR(VLP-16) → lidar_relay → FAST-LIVO2(LIO) → /aft_mapped_to_init
                                                  → vision_pose_relay → /mavros/vision_pose/pose
```
- FAST-LIVO2 有效特征: ~600-700 effective features/frame (初始帧 0→617，后续帧波动 0~696)
- SLAM 位姿: FAST-LIVO2 持续输出 `/aft_mapped_to_init`（带位置/姿态估计）
- Vision Pose: vision_pose_relay 成功转发到 `/mavros/vision_pose/pose` ✅
- PX4 MAVROS 配置: `EKF2_GPS_CTRL=0`(禁用GPS) `EKF2_HGT_REF=3`(视觉高度) `EKF2_EV_CTRL=15`(全视觉融合)

### 待做
- [ ] EKF2 视觉融合调参：ekf2 missing data 预检警告待解决（需 `COM_ARM_EKF` 阈值匹配）
- [ ] 多机 SLAM 地图共享 (Phase 9)
- [ ] SLAM + 任务 + MAPPO 全链路 (Phase 10)

### 新文件
- `ros2_ws/src/zcw_offboard/scripts/vision_pose_relay.py`: FAST-LIVO2 位姿 → MAVROS 视觉位姿转发

### 配置文件
- `scripts/start_livo_full.sh`: 更新为单模型方案（含 relay + vision_pose_relay 自动启动）
- `livo_config.yaml`: 参数完整（含 extrin_calib, preprocess, common）

### 持久记忆更新
- FAST-LIVO2 必须 LIO 模式（需要 MAVROS IMU），pure LO 有 sync 问题
- `lidar_relay.py` 必须输出6字段（含 time/ring）否则 PCL 转换失败
- MAVROS vision_pose 插件自动转发 `/mavros/vision_pose/pose` 到 PX4
- 环境变量: `LD_LIBRARY_PATH` 需含 `/opt/ros/humble/lib`(libfmt) + 排除 conda lib 冲突
