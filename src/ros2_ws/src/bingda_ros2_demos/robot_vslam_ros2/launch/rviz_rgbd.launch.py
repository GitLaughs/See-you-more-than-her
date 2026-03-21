#!/usr/bin/env python3

import launch
from launch.substitutions import Command, LaunchConfiguration
import launch_ros
import os

def generate_launch_description():
    pkg_share = launch_ros.substitutions.FindPackageShare(package='robot_vslam_ros2').find('robot_vslam_ros2')

    default_rviz_config_path = os.path.join(pkg_share, 'rviz/rgbd_cameras.rviz')

    rviz_node = launch_ros.actions.Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', LaunchConfiguration('rvizconfig')],
    )

    return launch.LaunchDescription([
        launch.actions.DeclareLaunchArgument(name='rvizconfig', default_value=default_rviz_config_path,
                                            description='Absolute path to rviz config file'),

        # robot_localization_node,
        rviz_node
    ])
