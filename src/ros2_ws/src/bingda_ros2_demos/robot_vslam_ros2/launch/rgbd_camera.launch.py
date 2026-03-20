#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import ThisLaunchFileDir
from launch.actions import IncludeLaunchDescription,DeclareLaunchArgument
from ament_index_python.packages import get_package_share_directory
import os
import launch.substitutions

robot_vslam_pkg_dir = get_package_share_directory('robot_vslam_ros2')

def generate_launch_description():

    try:
        RGBD_TYPE = os.environ['RGBD_TYPE']
    except:
        RGBD_TYPE = 'astra_pro'
        print(f"\033[91m Warning:Please Set The Correct RGBD_TYPE,Now USE Default: {RGBD_TYPE}\033[0m")

    try:
        ROBOT_TYPE = os.environ['BASE_TYPE']
    except:
        ROBOT_TYPE = 'NanoRobot'
        # print(f"\033[91m Warning:Please set the correct BASE_TYPE. Now using default: {ROBOT_TYPE}\033[0m")

    static_transform_args = ['0', '0', '0', '0', '0', '0', 'base_link', 'sensor_link']

    if ROBOT_TYPE == 'NanoRobot':
        static_transform_args = ['0.0', '0', '0.14', '0', '0', '0', 'base_link', 'camera_link']
    elif ROBOT_TYPE == 'NanoRobot_Pro':
        static_transform_args = ['0.0', '0', '0.14', '0', '0', '0', 'base_link', 'camera_link']        
    elif ROBOT_TYPE == 'NanoCar':
        static_transform_args = ['0.14', '0', '0.09', '0', '0', '0', 'base_link', 'camera_link']
    elif ROBOT_TYPE == 'NanoOmni':
        static_transform_args = ['0.09', '0', '0.08', '0', '0', '0', 'base_link', 'camera_link']        
    elif ROBOT_TYPE == 'Crab':
        static_transform_args = ['0.14', '0', '0.014', '0', '0', '0', 'base_link', 'camera_link']   
    elif ROBOT_TYPE == 'Panda':
        static_transform_args = ['0.11', '0', '0.48', '0', '0', '0', 'base_link', 'camera_link']  
    elif ROBOT_TYPE == 'PandaOmni':
        static_transform_args = ['0.11', '0', '0.48', '0', '0', '0', 'base_link', 'camera_link']                       
    else:
        print(f"rgbd Unknown ROBOT_TYPE: {ROBOT_TYPE}, using default transform")

    enable_pcd = launch.substitutions.LaunchConfiguration('enable_pcd', default='false')
    low_resolution = launch.substitutions.LaunchConfiguration('low_resolution', default='true')

    if low_resolution:
        image_width = '320'
        image_height ='240'
        ir_info_url= 'file://' + os.path.join(robot_vslam_pkg_dir, 'config', RGBD_TYPE, 'depth_camera_240.yaml')
        color_info_url=  'file://' + os.path.join(robot_vslam_pkg_dir, 'config', RGBD_TYPE, 'rgb_camera_240.yaml')
    else:
        image_width = '640'
        image_height ='480'
        ir_info_url= 'file://' + os.path.join(robot_vslam_pkg_dir, 'config', RGBD_TYPE, 'depth_camera.yaml')
        color_info_url=  'file://' + os.path.join(robot_vslam_pkg_dir, 'config', RGBD_TYPE, 'rgb_camera.yaml')     

    return LaunchDescription([

        IncludeLaunchDescription(
            AnyLaunchDescriptionSource([robot_vslam_pkg_dir,'/launch', '/'+RGBD_TYPE+'.launch.xml']),
            launch_arguments={'enable_point_cloud': enable_pcd,
                              'enable_ir': enable_pcd,
                              'enable_colored_point_cloud':enable_pcd,
                              'depth_width':image_width,
                              'depth_height':image_height,
                              'color_width':image_width,
                              'color_height':image_height,
                              'ir_width':image_width,
                              'ir_height':image_height, 
                              'ir_info_url':ir_info_url,
                              'color_info_url':color_info_url,                                                           
                              }.items(),
        ),         
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_to_rgbd_camera',
            arguments=static_transform_args,  # set tf transform data
            output='screen'
        ),            
    ])


