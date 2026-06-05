# Gazebo GUI 启动注意事项

## 问题
`make px4_sitl gazebo-classic_iris` 启动的 `gzclient` 因 OGRE 渲染错误崩溃：
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
