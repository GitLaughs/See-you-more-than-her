
#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import ThisLaunchFileDir
from launch.actions import IncludeLaunchDescription
from ament_index_python.packages import get_package_share_directory
import os

base_control_pkg_dir = LaunchConfiguration(
    'base_control_pkg_dir',
    default=os.path.join(get_package_share_directory('base_control_ros2'))
    )

robot_vslam_pkg_dir = LaunchConfiguration(
    'robot_vslam_pkg_dir',
    default=os.path.join(get_package_share_directory('robot_vslam_ros2'))
    )

def generate_launch_description():

    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([base_control_pkg_dir, '/launch', '/base_control.launch.py']),
        ),    
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([robot_vslam_pkg_dir,'/launch', '/rgbd_camera.launch.py']),
        ),            
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([robot_vslam_pkg_dir,'/launch', '/depthimage_to_laserscan.launch.py']),
        ),                                    
    ])


