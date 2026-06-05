# Gazebo GUI 启动注意事项

## 问题
Gazebo 图形界面（gzclient）黑屏闪退。

## 可能原因

### 原因 1：环境变量缺失
`source /opt/ros/humble/setup.bash` 不会设置 `GAZEBO_RESOURCE_PATH` 和 `OGRE_RESOURCE_PATH`，导致 Gazebo 找不到必要的渲染资源。

### 原因 2：BS 项目库冲突
`~/.bashrc:149` 中 `source /home/travis/zcw/BS/ros2_ws/install/setup.bash` 会向 `LD_LIBRARY_PATH` 追加 BS 项目的老版本 ROS/Gazebo 库路径，优先级高于系统库，OGRE 加载冲突的 `.so` 后 GPU 渲染直接崩溃。

### 原因 3：TL 模型物理过载（已修复）
TL 模型的 42 个 STL 碰撞网格（21/塔 × 2 塔）导致 ODE 物理引擎初始化时卡死。已在模型上添加 `<static>true</static>`。

## 修复方案

### 使用项目专用 setup.bash
```bash
source /home/travis/zcw/1.2/setup.bash
```
该脚本会自动剔除 `*/BS/*` 路径，然后 source ROS 2 + Gazebo + PX4 + 工作空间。

### 快捷命令
```bash
zcw-px4         # 启动 PX4 SITL（tower 场景，HEADLESS）
zcw-gzclient    # 启动 Gazebo GUI
zcw-offboard    # 启动 offboard 控制
```

## 正确启动步骤

### 1. 推荐方式
```bash
# 终端 1: PX4 SITL
source /home/travis/zcw/1.2/setup.bash
zcw-px4

# 终端 2: Gazebo GUI
source /home/travis/zcw/1.2/setup.bash
zcw-gzclient

# 终端 3: Offboard 控制
source /home/travis/zcw/1.2/setup.bash
zcw-offboard
```

### 2. 手动方式
```bash
# 终端 1: PX4 SITL
source /opt/ros/humble/setup.bash     # 先 ROS 2
source /usr/share/gazebo/setup.sh     # 再 Gazebo
cd PX4-Autopilot
HEADLESS=1 make px4_sitl gazebo-classic_iris    # 只起 gzserver

# 终端 2: Gazebo GUI
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
gzclient --verbose &
```

## 错误做法
```bash
# ❌ 直接用 make 启动 gzclient
make px4_sitl gazebo-classic_iris    # 缺环境变量崩溃

# ❌ BS 路径污染
# 如果终端已经 source 过 BS 的 setup.bash，必须用 setup.bash 清理

# ❌ 只 source ROS 不 source Gazebo
source /opt/ros/humble/setup.bash
gzclient    # 缺少 GAZEBO_RESOURCE_PATH
```
[Err] [RTShaderSystem.cc:480] Unable to find shader lib.
[Err] [RenderEngine.cc:197] Failed to initialize scene
Assertion 'px != 0' failed. (boost shared_ptr)
```

## 根因
- PX4 的 `make` 命令内部调起 gzclient 时，子 shell 环境缺少 `GAZEBO_RESOURCE_PATH` 和 `OGRE_RESOURCE_PATH`
- `source /opt/ros/humble/setup.bash` **不包含** Gazebo 路径设置，需要额外执行 `source /usr/share/gazebo/setup.sh`

## 正确启动方式

### 终端 1：Gazebo 服务器 + PX4 SITL
```bash
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
export PX4_ROOT=/home/travis/zcw/1.2/PX4-Autopilot

# 只用 HEADLESS 启动 gzserver（不启动 gzclient）
cd ${PX4_ROOT}
HEADLESS=1 make px4_sitl gazebo-classic_iris
```

### 终端 2：Gazebo GUI（单独启动）
```bash
source /opt/ros/humble/setup.bash
source /usr/share/gazebo/setup.sh
gzclient --verbose &
```

### 终端 3：MAVROS + Offboard 控制
```bash
source /opt/ros/humble/setup.bash
source /home/travis/zcw/1.2/ros2_ws/install/local_setup.bash
ros2 run mavros mavros_node --ros-args -p fcu_url:=udp://:14540@127.0.0.1:14580 &
ros2 run zcw_offboard offboard_control
```

## 环境变量要点
| 变量 | 来源 | 作用 |
|---|---|---|
| `GAZEBO_RESOURCE_PATH` | `source /usr/share/gazebo/setup.sh` | 定位 Gazebo 材质和着色器 |
| `OGRE_RESOURCE_PATH` | `source /usr/share/gazebo/setup.sh` | 定位 OGRE 插件（RTShaderSystem 等） |
| `GAZEBO_PLUGIN_PATH` | 需追加 PX4 构建路径 | 加载 PX4 Gazebo 插件 |
| `GAZEBO_MODEL_PATH` | 需追加 Iris 模型路径 | 加载无人机模型 |
| `LD_LIBRARY_PATH` | 同上 | 共享库搜索路径 |

## 快速验证脚本
```bash
./scripts/run_minimal_test.sh
```
该脚本会在终端前台运行，启动完整闭环测试。
