# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This is all-in-one launch script intended for use by nav2 developers."""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from geometry_msgs.msg import PoseWithCovarianceStamped


def generate_launch_description():

    try:
        ROBOT_TYPE = os.environ['BASE_TYPE']
    except:
        ROBOT_TYPE = 'NanoRobot'
        print(f"\033[91m Warning:Please set the correct BASE_TYPE. Now using default: {ROBOT_TYPE}\033[0m")

    # Get the launch directory
    bringup_dir = get_package_share_directory('robot_navigation_ros2')
    launch_dir = os.path.join(bringup_dir, 'launch')
    robot_navigation_ros2_dir = get_package_share_directory('robot_navigation_ros2')
    # Create the launch configuration variables
    localization = LaunchConfiguration('localization')
    namespace = LaunchConfiguration('namespace')
    use_namespace = LaunchConfiguration('use_namespace')
    map_yaml_file = LaunchConfiguration('map')
    use_sim_time = LaunchConfiguration('use_sim_time')
    params_file = LaunchConfiguration('params_file')
    autostart = LaunchConfiguration('autostart')
    use_composition = LaunchConfiguration('use_composition')
    use_respawn = LaunchConfiguration('use_respawn')

    # Declare the launch arguments
    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace',
        default_value='',
        description='Top-level namespace')

    declare_use_namespace_cmd = DeclareLaunchArgument(
        'use_namespace',
        default_value='false',
        description='Whether to apply a namespace to the navigation stack')

    declare_slam_cmd = DeclareLaunchArgument(
        'localization',
        default_value='True',
        description='Whether run a localization')

    declare_map_yaml_cmd = DeclareLaunchArgument(
        'map',
        default_value=os.path.join(
            robot_navigation_ros2_dir, 'map', 'map.yaml'),
        description='Full path to map file to load')

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation (Gazebo) clock if true')
    
    # if(use_sim_time == 'false'):
    #     declare_map_yaml_cmd = DeclareLaunchArgument(
    #     'map',
    #     default_value=os.path.join(
    #         robot_navigation_ros2_dir, 'map', 'map.yaml'),
    #     description='Full path to map file to load')
    # else:
    #     declare_map_yaml_cmd = DeclareLaunchArgument(
    #     'map',
    #     default_value=os.path.join(
    #         robot_navigation_ros2_dir, 'map', 'map_gz.yaml'),
    #     description='Full path to map file to load')

    declare_params_file_cmd = DeclareLaunchArgument(
        'params_file',
        default_value=os.path.join(bringup_dir, 'params', ROBOT_TYPE + '.yaml'),
        description='Full path to the ROS2 parameters file to use for all launched nodes')

    
    declare_autostart_cmd = DeclareLaunchArgument(
        'autostart', default_value='true',
        description='Automatically startup the nav2 stack')

    declare_use_composition_cmd = DeclareLaunchArgument(
        'use_composition', default_value='True',
        description='Whether to use composed bringup')

    declare_use_respawn_cmd = DeclareLaunchArgument(
        'use_respawn', default_value='False',
        description='Whether to respawn if a node crashes. Applied when composition is disabled.')

    declare_rviz_config_file_cmd = DeclareLaunchArgument(
        'rviz_config_file',
        default_value=os.path.join(
            bringup_dir, 'rviz', 'bingda_navigation.rviz'),
        description='Full path to the RVIZ config file to use')

    # declare_use_simulator_cmd = DeclareLaunchArgument(
    #     'use_simulator',
    #     default_value='True',
    #     description='Whether to start the simulator')

    # declare_use_robot_state_pub_cmd = DeclareLaunchArgument(
    #     'use_robot_state_pub',
    #     default_value='True',
    #     description='Whether to start the robot state publisher')

    declare_use_rviz_cmd = DeclareLaunchArgument(
        'use_rviz',
        default_value='True',
        description='Whether to start RVIZ')

    bringup_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(launch_dir, 'bringup_launch.py')),
        launch_arguments={'namespace': namespace,
                          'use_namespace': use_namespace,
                          'localization': localization,
                          'map': map_yaml_file,
                          'use_sim_time': use_sim_time,
                          'params_file': params_file,
                          'autostart': autostart,
                          'use_composition': use_composition,
                          'use_respawn': use_respawn}.items())

    # Create the launch description and populate
    ld = LaunchDescription()

    # Declare the launch options
    ld.add_action(declare_namespace_cmd)
    ld.add_action(declare_use_namespace_cmd)
    ld.add_action(declare_slam_cmd)
    ld.add_action(declare_map_yaml_cmd)
    ld.add_action(declare_use_sim_time_cmd)
    ld.add_action(declare_params_file_cmd)
    ld.add_action(declare_autostart_cmd)
    ld.add_action(declare_use_composition_cmd)

    ld.add_action(declare_rviz_config_file_cmd)
    # ld.add_action(declare_use_simulator_cmd)
    # ld.add_action(declare_use_robot_state_pub_cmd)
    ld.add_action(declare_use_rviz_cmd)
    ld.add_action(declare_use_respawn_cmd)
    ld.add_action(bringup_cmd)

    # ld.add_action(pub_initial_pose_cmd)

    return ld
