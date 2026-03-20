from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='a1_robot_stack',
            executable='perception_node',
            name='perception_node',
            output='screen',
            parameters=[{
                'target_fps': 20.0,
                'use_mock_input': False,
                'camera_topic': '/a1/camera/mono',
                'camera_timeout_sec': 0.5,
            }],
        ),
        Node(
            package='a1_robot_stack',
            executable='lidar_ingest_node',
            name='lidar_ingest_node',
            output='screen',
            parameters=[{
                'obstacle_threshold_m': 0.7,
                'use_scan_topic': False,
                'use_rplidar_sdk': True,
                'serial_port': '/dev/ttyUSB0',
                'serial_baud': 230400,
            }],
        ),
        Node(
            package='a1_robot_stack',
            executable='chassis_controller_node',
            name='chassis_controller_node',
            output='screen',
            parameters=[{
                'max_linear': 0.35,
                'max_angular': 1.0,
                'use_uart_output': True,
                'uart_port': '/dev/ttyS0',
                'uart_baud': 115200,
            }],
        ),
        Node(
            package='a1_robot_stack',
            executable='display_bridge_node',
            name='display_bridge_node',
            output='screen',
            parameters=[{
                'publish_period_ms': 300,
                'status_file_path': '/tmp/a1_display_status.txt',
            }],
        ),
        Node(package='a1_robot_stack', executable='safety_supervisor_node', name='safety_supervisor_node', output='screen'),
    ])
