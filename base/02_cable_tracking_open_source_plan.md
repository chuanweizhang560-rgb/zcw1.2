# 电缆巡检感知与跟踪专项计划

## 概述

本文件记录电缆巡检场景中感知与跟踪算法的开源方案调研、选型与集成计划。

## 技术路线（第一版）

- 点云预处理：PCL (voxel filter, passthrough, statistical outlier removal)
- 导线候选提取：PCL RANSAC line model
- 中心线重建：catenary / spline 拟合
- 局部跟踪：Frenet frame + pure pursuit
- 重捕获：扇形 / 平行扫描

## 候选开源仓库

| 仓库 | 用途 | 许可证 | 状态 |
|---|---|---|---|
| (待调研) | | | |

## 集成计划

1. 离线验证 PCL RANSAC 导线提取
2. 构建 Frenet 跟踪节点
3. 接入重捕获状态机
4. 场景闭环测试

_状态：未开始_
