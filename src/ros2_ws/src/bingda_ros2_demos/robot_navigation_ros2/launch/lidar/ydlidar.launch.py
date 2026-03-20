#!/usr/bin/python3

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch_ros.actions import LifecycleNode
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.actions import LogInfo

import lifecycle_msgs.msg
import os

def generate_launch_description():
    try:
        BASE_TYPE = os.environ['BASE_TYPE']
    except:
        BASE_TYPE = 'NanoRobot'
        
    share_dir = get_package_share_directory('robot_navigation_ros2')
    parameter_file = LaunchConfiguration('params_file')



    params_declare = DeclareLaunchArgument('params_file',
                                           default_value=os.path.join(
                                               share_dir, 'config', 'S4.yaml'),
                                           description='FPath to the ROS2 parameters file to use.')

    driver_node = LifecycleNode(package='ydlidar_ros2_driver',
                                executable='ydlidar_ros2_driver_node',
                                name='ydlidar_ros2_driver_node',
                                output='screen',
                                emulate_tty=True,
                                parameters=[parameter_file],
                                namespace='/',
                                )

    return LaunchDescription([
        params_declare,
        driver_node,
    ])