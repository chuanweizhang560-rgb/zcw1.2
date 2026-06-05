# zcw

新的独立研究仓库，目标是从零搭建"风机巡检 + 电缆巡检"的多无人机协同感知与规划平台。

当前约束：

- 只做风机巡检和电缆巡检，不做光伏。
- 不复用现有 `BS` 仓库的实现代码，只把它当作历史路线参考。
- 采用开源成熟方案优先，低层飞控不自研。
- 第一版默认围绕 `PX4 SITL + ROS 2 Humble + Gazebo 11` 组织。
- 过程日志见 [PROCESS_LOG.md](PROCESS_LOG.md)。
- 第一阶段开源审计见 [OPEN_SOURCE_AUDIT.md](OPEN_SOURCE_AUDIT.md)。
- 当前执行入口见 [RUNBOOK.md](RUNBOOK.md)。

详细工作流见 [docs/00_workflow.md](docs/00_workflow.md)。
电缆巡检感知与跟踪专项计划见 [docs/02_cable_tracking_open_source_plan.md](docs/02_cable_tracking_open_source_plan.md)。
