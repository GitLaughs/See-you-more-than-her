#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import LifecycleNode
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument
import os
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    try:
        ROBOT_TYPE = os.environ['BASE_TYPE']
    except:
        ROBOT_TYPE = 'NanoRobot'
        print(f"\033[91m Warning:Please set the correct BASE_TYPE. Now using default: {ROBOT_TYPE}\033[0m")

    imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'sensor_link']

    if ROBOT_TYPE == 'NanoRobot':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']
    elif ROBOT_TYPE == 'NanoRobot_Pro':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']        
    elif ROBOT_TYPE == 'NanoCar':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']
    elif ROBOT_TYPE == 'NanoOmni':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']  
    elif ROBOT_TYPE == 'Crab':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']
    elif ROBOT_TYPE == 'Panda':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']
    elif ROBOT_TYPE == 'PandaOmni':
        imu_static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'imu']                
    else:
        print(f"Unknown ROBOT_TYPE: {ROBOT_TYPE}, using default transform")
    base_footprint_static_transform_args = ['0.0', '0.0', '0.0', '0.0', '0.0', '0.0', 'base_footprint', 'base_link'] 
    return LaunchDescription([      
        LifecycleNode(
            package='base_control_ros2',
            executable='base_control_node',
            name='base_control_ros2',
            output='screen',
            namespace=''
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_to_imu',
            arguments=imu_static_transform_args,  # set tf transform data
            output='log'
        ),      
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_to_footprint',
            arguments=base_footprint_static_transform_args,  # set tf transform data
            output='log'
        ),                  
    ])
