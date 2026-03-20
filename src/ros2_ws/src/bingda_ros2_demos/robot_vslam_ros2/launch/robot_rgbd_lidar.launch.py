
#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import ThisLaunchFileDir
from launch.actions import IncludeLaunchDescription
from ament_index_python.packages import get_package_share_directory
from launch.actions import TimerAction
import os

base_control_pkg_dir = LaunchConfiguration(
    'base_control_pkg_dir',
    default=os.path.join(get_package_share_directory('base_control_ros2'))
    )

robot_navigation_pkg_dir = LaunchConfiguration(
    'robot_navigation_pkg_dir',
    default=os.path.join(get_package_share_directory('robot_navigation_ros2'))
    )

robot_vslam_pkg_dir = LaunchConfiguration(
    'rrobot_vslam_pkg_dir',
    default=os.path.join(get_package_share_directory('robot_vslam_ros2'))
    )

# def generate_launch_description():

#     return LaunchDescription([
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource([base_control_pkg_dir, '/launch', '/base_control.launch.py']),
#         ),
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource([robot_navigation_pkg_dir,'/launch', '/lidar.launch.py']),
#         ),          
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource([robot_vslam_pkg_dir,'/launch', '/rgbd_camera.launch.py']),
#         ),                    
#     ])
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    return LaunchDescription([
        # 启动第一个 launch 文件
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([base_control_pkg_dir, '/launch', '/base_control.launch.py']),
        ),
        
        # 插入一个延迟 3 秒的 TimerAction
        TimerAction(
            period=3.0,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource([robot_navigation_pkg_dir, '/launch', '/lidar.launch.py']),
                ),
            ]
        ),
        
        # 再插入另一个延迟 5 秒的 TimerAction
        TimerAction(
            period=5.0,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource([robot_vslam_pkg_dir, '/launch', '/rgbd_camera.launch.py']),
                ),
            ]
        ),
    ])


