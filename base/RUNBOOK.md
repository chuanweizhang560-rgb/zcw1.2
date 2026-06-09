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
- [ ] **阶段 6**：四机协同 ← **下一阶段**
- [ ] **阶段 7**：RL 接入

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
