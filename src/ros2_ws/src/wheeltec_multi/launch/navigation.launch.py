import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, GroupAction,
                            IncludeLaunchDescription, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
import launch_ros.actions


def generate_launch_description():
    nav_dir = get_package_share_directory('wheeltec_nav2')
    nav_launch_dir = os.path.join(nav_dir,'launch')
#开启导航节点，用于小车定位
    nav = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(nav_launch_dir,'wheeltec_nav2.launch.py')),
    )
#分享主车位置与速度信息给其他车辆
    send_tfodom = launch_ros.actions.Node(
            package='wheeltec_multi', 
            executable='send_tfodom.py', 
            name='send_tfodom',
            output='screen'
    )



    return LaunchDescription([
        nav,send_tfodom
    ])

