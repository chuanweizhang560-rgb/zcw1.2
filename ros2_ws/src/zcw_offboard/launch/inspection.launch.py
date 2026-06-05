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
            executable='inspection_control',
            name='inspection_control',
            parameters=[{
                'radius': 80.0,
                'height': 60.0,
                'angular_velocity': 0.15,
                'center_x': 80.0,
                'center_y': 0.0,
            }],
            output='screen',
        ),
    ])
