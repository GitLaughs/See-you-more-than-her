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
                'use_mock_input': False,
                'camera_topic': '/a1/camera/mono',
            }],
        ),
        Node(
            package='a1_robot_stack',
            executable='lidar_ingest_node',
            name='lidar_ingest_node',
            output='screen',
            parameters=[{
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
                'status_file_path': '/tmp/a1_display_status.txt',
            }],
        ),
        Node(package='a1_robot_stack', executable='safety_supervisor_node', name='safety_supervisor_node', output='screen'),
        Node(package='a1_robot_stack', executable='performance_monitor_node', name='performance_monitor_node', output='screen'),
    ])
