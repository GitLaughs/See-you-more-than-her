from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='rplidar_ros2',
            executable='rplidar_ros2_node',
            name='rplidar_ros2_node',
            output='screen',
            parameters=[{
                'port': '/dev/ttyUSB0',
                'baudrate': 115200,
                'frame_id': 'laser_frame',
                'range_min': 0.1,
                'range_max': 64.0,
                'publish_rate_hz': 10.0,
            }]
        )
    ])
