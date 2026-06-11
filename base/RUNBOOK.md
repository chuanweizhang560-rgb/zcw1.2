# ZCW 执行入口

## 当前状态

- [x] **阶段 0**：仓库骨架
- [x] **阶段 1**：资产审计
- [x] **阶段 2**：仿真环境最小闭环
- [x] **阶段 3**：单机风机巡检
- [x] **阶段 4.1**：导线中心线先验飞行
- [x] **阶段 4.2**：点云线拟合离线验证
- [x] **阶段 4.3**：在线中心线跟随（含重捕获）
- [x] **阶段 4.4**：失锁回退
- [x] **阶段 5**：双机中继
- [x] **阶段 6**：四机协同 ✅
- [x] **阶段 7**：RL 接入 ✅
- [x] **Phase 8**：后续改进（沙漠场景完善 + 全栈测试）✅
- [x] **Phase 8**：SLAM 开源方案调研与集成 ✅（FAST-LIVO2 LIO-only 跑通）
- [ ] **Phase 9**：增量建图 + 多机地图共享
- [ ] **Phase 10**：SLAM + 建图 + 任务 + MAPPO 完整链路

## 快速启动（沙漠世界生成器）

```bash
source /home/travis/zcw/1.2/setup.bash

# 生成并启动沙漠世界（seed=42, 8风机, 50m起伏）
python3 /home/travis/zcw/1.2/scripts/world_gen/generate_desert_world.py \
  --output /home/travis/zcw/1.2/assets/worlds/generated/seed_42 \
  --seed 42 --turbine-count 8 --terrain-height 50

# 启动 GUI
gzclient --verbose
```

```bash
source /home/travis/zcw/1.2/setup.bash

# 终端 1: PX4 + Gazebo (沙漠地形)
zcw-px4-cable-desert

# 终端 2: GUI
zcw-gzclient-follow

# 终端 3: MAVROS
zcw-mavros

# 终端 4: 跟踪控制器
zcw-cable-track
```

## 快速启动（电缆巡检场景）

```bash
source /home/travis/zcw/1.2/setup.bash

# 单机模式 (Phase 4)
# 终端 1: PX4 SITL
zcw-px4-cable

# 终端 2: Gazebo GUI（自动追踪）
zcw-gzclient-follow

# 终端 3: MAVROS
zcw-mavros

# 终端 4: 跟踪控制器
zcw-cable-track
```

## 快速启动（风机巡检场景）

```bash
source /home/travis/zcw/1.2/setup.bash

# 终端 1: PX4 SITL
zcw-px4-turbine

# 终端 2: Gazebo GUI（自动追踪无人机）
zcw-gzclient-follow

# 终端 3: MAVROS
ros2 launch mavros px4.launch fcu_url:="udp://:14540@127.0.0.1:14580"

# 终端 4: 巡检控制器
zcw-inspect
```

## 快速启动（双机巡检+中继场景 / Phase 5）

```bash
source /home/travis/zcw/1.2/setup.bash

# 选项 A: 自动脚本（推荐）
bash scripts/start_dual_sim.sh

# 选项 B: 手动
# 终端 1: PX4 SITL + 模型（需手动运行 gzserver + 2x PX4 + 2x MAVROS）
# 终端 2: zcw-inspect-dual  (INSPECT 巡检控制器)
# 终端 3: zcw-relay         (RELAY 中继控制器)
```

## 快速启动（RL 电缆跟踪 / Phase 7）

```bash
source /home/travis/zcw/1.2/setup.bash

# 终端 1: PX4 + Gazebo
zcw-px4-cable

# 终端 2: GUI
zcw-gzclient-follow

# 终端 3: MAVROS
zcw-mavros

# 终端 4: PPO 跟踪
zcw-track-rl
```

## 快速启动（FAST-LIVO2 SLAM / Phase 8）

```bash
source /home/travis/zcw/1.2/setup.bash

# 方式 1: 一键全栈验证（推荐）
# 启动: gzserver → iris_stereo_velodyne → PX4 → MAVROS → lidar_relay → vision_pose_relay → FAST-LIVO2
zcw-livo-full

# 方式 2: 单机启动（需已有传感器数据）
zcw-livo

# 模型: iris_stereo_velodyne (VLP-16 + 双目 + IMU)
# LiDAR: /velodyne_laser_plugin/out → lidar_relay → /velodyne/points_raw (6字段: x,y,z,intensity,ring,time)
# IMU: /mavros/imu/data (50Hz, MAVROS 必须用 ros2 run 直接启动)
# SLAM 输出: /aft_mapped_to_init /cloud_registered /Laser_map /path /LIVO2/imu_propagate
# Vision Pose: /aft_mapped_to_init → vision_pose_relay → /mavros/vision_pose/pose
# 注意事项:
#   - FAST-LIVO2 + MAVROS 都需 use_sim_time:=True
#   - config 需完整 extrin_calib
#   - LD_LIBRARY_PATH 需含 /opt/ros/humble/lib (libfmt)
#   - FAST-LIVO2 需 LIO 模式 (MAVROS IMU 必须运行), pure LO 模式有 sync 问题
#   - lidar_relay 必须输出6字段含 time+ring, 否则 PCL 转换失败
```

## 快速启动（四机协同 / Phase 6）

```bash
source /home/travis/zcw/1.2/setup.bash

# 一键启动四机仿真（推荐）
zcw-4uav
```

## 关键文档

- [工作流](docs/00_workflow.md)
- [过程日志](PROCESS_LOG.md)
- [开源审计](OPEN_SOURCE_AUDIT.md)

## 环境

| 组件 | 版本 | 状态 |
|---|---|---|
| ROS 2 | Humble | ✅ |
| Gazebo | 11.10.2 | ✅ |
| PX4 SITL | v1.14.0 | ✅ |
| Ubuntu | 22.04 | ✅ |
| GPU | RTX 4060 (8GB) | ✅ |
