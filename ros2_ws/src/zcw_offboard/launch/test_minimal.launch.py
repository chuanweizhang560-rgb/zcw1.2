"""ZCW Phase 2 最小闭环测试启动文件

用法:
  ros2 launch zcw_offboard test_minimal.launch.py

前提:
  PX4 SITL + Gazebo 需已在运行:
    cd PX4-Autopilot && make px4_sitl gazebo-classic_iris
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='mavros',
            executable='mavros_node',
            name='mavros_node',
            parameters=[{
                'fcu_url': 'udp://:14540@127.0.0.1:14580',
                'system_id': 1,
                'component_id': 1,
            }],
            output='screen',
        ),
        Node(
            package='zcw_offboard',
            executable='offboard_control',
            name='offboard_control',
            output='screen',
        ),
    ])
