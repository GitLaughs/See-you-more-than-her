import os
from pathlib import Path
from launch import LaunchDescription
from ament_index_python.packages import get_package_share_directory
import launch_ros.actions
from launch.substitutions import EnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.conditions import UnlessCondition


def generate_launch_description():
    ekf_config = Path(get_package_share_directory('turn_on_wheeltec_robot'), 'config', 'ekf.yaml')
    ekf_carto_config = Path(get_package_share_directory('turn_on_wheeltec_robot'), 'config', 'ekf_carto.yaml')

    carto_slam = LaunchConfiguration('carto_slam', default='false')
    carto_slam_dec = DeclareLaunchArgument('carto_slam',default_value='false')
             
    return LaunchDescription([
    carto_slam_dec,
            Node(
                condition=IfCondition(carto_slam),
                package='robot_localization',
                executable='ekf_node',
                name='carto_ekf_filter_node',
                parameters=[ekf_carto_config],
                remappings=[('/odometry/filtered','odom_combined')]
               ),

            Node(
                condition=UnlessCondition(carto_slam),
                package='robot_localization',
                executable='ekf_node',
                name='ekf_filter_node',
                parameters=[ekf_config],
                remappings=[('/odometry/filtered','odom_combined')]
               ),
])
