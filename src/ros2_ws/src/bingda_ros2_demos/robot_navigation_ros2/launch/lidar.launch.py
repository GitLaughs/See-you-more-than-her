#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import ThisLaunchFileDir
from launch.actions import IncludeLaunchDescription
from ament_index_python.packages import get_package_share_directory
import os



robot_navigation_pkg_dir = LaunchConfiguration(
    'robot_navigation_pkg_dir',
    default=os.path.join(get_package_share_directory('robot_navigation_ros2'))
    )


def generate_launch_description():

    try:
        LIDAR_TYPE = os.environ['LIDAR_TYPE']
    except:
        LIDAR_TYPE = 'rplidar_a1'
        print(f"\033[91m Warning:Please Set The Correct LIDAR_TYPE,Now USE Default: {LIDAR_TYPE}\033[0m")

    try:
        ROBOT_TYPE = os.environ['BASE_TYPE']
    except:
        ROBOT_TYPE = 'NanoRobot'
        # print(f"\033[91m Warning:Please set the correct BASE_TYPE. Now using default: {ROBOT_TYPE}\033[0m")

    static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'base_laser_link']

    if ROBOT_TYPE == 'NanoRobot':
        static_transform_args = ['-0.01', '0', '0.18', '3.14159', '0', '0', 'base_link', 'base_laser_link']
    elif ROBOT_TYPE == 'NanoRobot_Pro':
        static_transform_args = ['-0.01', '0', '0.18', '-1.57', '0', '0', 'base_link', 'base_laser_link']
    elif ROBOT_TYPE == 'NanoCar':
        static_transform_args = ['0.1', '0', '0.115', '3.14159', '0', '0', 'base_link', 'base_laser_link']
    elif ROBOT_TYPE == 'NanoOmni':
        static_transform_args = ['0.05', '0', '0.16', '3.14159', '0', '0', 'base_link', 'base_laser_link']       
    elif ROBOT_TYPE == 'Crab':
        static_transform_args = ['0.05', '0', '0.26', '3.14159', '0', '0', 'base_link', 'base_laser_link']     
    elif ROBOT_TYPE == 'Panda':
        static_transform_args = ['0.01', '0', '0.52', '3.14159', '0', '0', 'base_link', 'base_laser_link'] 
    elif ROBOT_TYPE == 'PandaOmni':
        static_transform_args = ['0.01', '0', '0.52', '3.14159', '0', '0', 'base_link', 'base_laser_link']                         
    else:
        print(f"lidar Unknown ROBOT_TYPE: {ROBOT_TYPE}, using default transform")

    return LaunchDescription([

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([robot_navigation_pkg_dir,'/launch', '/'+LIDAR_TYPE+'.launch.py']),
            launch_arguments={'serial_port': '/dev/lidar'}.items(),
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_to_laser',
            arguments=static_transform_args,  # set tf transform data
            output='screen'
        ),                  
    ])


