# zcw

新的独立研究仓库，目标是从零搭建"风机巡检 + 电缆巡检"的多无人机协同感知与规划平台。

当前约束：

- 只做风机巡检和电缆巡检，不做光伏。
- 不复用现有 `BS` 仓库的实现代码，只把它当作历史路线参考。
- 采用开源成熟方案优先，低层飞控不自研。
- 第一版默认围绕 `PX4 SITL + ROS 2 Humble + Gazebo 11` 组织。
- 过程日志见 [PROCESS_LOG.md](PROCESS_LOG.md)，每个节点都必须及时记录。
- 第一阶段开源审计见 [OPEN_SOURCE_AUDIT.md](OPEN_SOURCE_AUDIT.md)。
- 当前执行入口见 [RUNBOOK.md](RUNBOOK.md)。
- 需要审核仿真界面运动时，我会给出审核确认点，由你来实时审核确认。

详细工作流见 [docs/00_workflow.md](docs/00_workflow.md)。
电缆巡检感知与跟踪专项计划见 [docs/02_cable_tracking_open_source_plan.md](docs/02_cable_tracking_open_source_plan.md)。
每次上下文压缩后先读取base了解项目进度
管理好项目文件夹分类，整齐
项目管理方面的就放在base，其他的你来安排
所有内容包括环境都放在该项目目录，包括一些ros环境什么的，不要放再tmp下
我本台电脑有其他仿真项目，在开发该项目时候不要影响到其他项目
- 远程仓库：`https://github.com/chuanweizhang560-rgb/zcw1.2.git`
- 本地分支：`zcw1.2`
- Python 环境：`zcw1.2`

