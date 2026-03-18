from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(package='a1_robot_stack', executable='perception_node', name='perception_node', output='screen'),
        Node(package='a1_robot_stack', executable='lidar_ingest_node', name='lidar_ingest_node', output='screen'),
        Node(package='a1_robot_stack', executable='chassis_controller_node', name='chassis_controller_node', output='screen'),
        Node(package='a1_robot_stack', executable='safety_supervisor_node', name='safety_supervisor_node', output='screen'),
        Node(package='a1_robot_stack', executable='performance_monitor_node', name='performance_monitor_node', output='screen'),
    ])
