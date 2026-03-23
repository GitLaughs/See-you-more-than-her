import os

from ament_index_python.packages import get_package_share_directory
import launch_ros.actions
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, SetEnvironmentVariable
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import LoadComposableNodes
from launch_ros.actions import Node
from launch_ros.descriptions import ComposableNode
from nav2_common.launch import RewrittenYaml

def generate_robot_node(robot_urdf,child):
    return launch_ros.actions.Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name=f'robot_state_publisher_{child}',
        arguments=[os.path.join(get_package_share_directory('wheeltec_robot_urdf'), 'urdf', robot_urdf)],
    )

def generate_static_transform_publisher_node(translation, rotation, parent, child):
    return launch_ros.actions.Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name=f'base_to_{child}',
        arguments=[translation[0], translation[1], translation[2], rotation[0], rotation[1], rotation[2], parent, child],
    )
    
def generate_launch_description():
    r3s_mec = LaunchConfiguration('r3s_mec', default='false')
    r3s_4wd = LaunchConfiguration('r3s_4wd', default='false')
    
    mini_mec = LaunchConfiguration('mini_mec', default='false')
    mini_akm = LaunchConfiguration('mini_akm', default='false')
    mini_tank = LaunchConfiguration('mini_tank', default='false')
    mini_4wd = LaunchConfiguration('mini_4wd', default='false')
    mini_diff = LaunchConfiguration('mini_diff', default='false')
    brushless_senior_diff = LaunchConfiguration('brushless_senior_diff', default='false')

    r3s_mec_ = GroupAction(
        condition=IfCondition(r3s_mec), 
        actions=[
        generate_robot_node('r3s_mec_robot.urdf','r3s_mec'),
        generate_static_transform_publisher_node(['0.05134 ', '0', '0.09452'], ['0', '0', '0'], 'base_footprint', 'laser'),
        generate_static_transform_publisher_node(['0.11365 ', '0.00017', '0.0762'], ['0', '0', '0'], 'base_footprint', 'camera_link'),    
    ]) 

    r3s_4wd_ = GroupAction(
        condition=IfCondition(r3s_4wd),
        actions=[
        generate_robot_node('r3s_4wd_robot.urdf','r3s_4wd'),
        generate_static_transform_publisher_node(['0.03163', '0.00009', '0.09502'], ['0', '0', '0'], 'base_footprint', 'laser'),
        generate_static_transform_publisher_node(['0.10709', '0.00032', '0.0762'], ['0', '0', '0'], 'base_footprint', 'camera_link'),    
    ])
    

    mini_akm_ = GroupAction(
        condition=IfCondition(mini_akm), 
        actions=[
        generate_robot_node('mini_akm_robot.urdf','mini_akm'),
        generate_static_transform_publisher_node(['0.05134 ', '0', '0.09452'], ['0', '0', '0'], 'base_footprint', 'laser'),
        generate_static_transform_publisher_node(['0.11365 ', '0.00017', '0.0762'], ['0', '0', '0'], 'base_footprint', 'camera_link'),    
    ]) 

    mini_mec_ = GroupAction(
        condition=IfCondition(mini_mec),
        actions=[
        generate_robot_node('mini_mec_robot.urdf','mini_mec'),
        generate_static_transform_publisher_node(['0.03163', '0.00009', '0.09502'], ['0', '0', '0'], 'base_footprint', 'laser'),
        generate_static_transform_publisher_node(['0.10709', '0.00032', '0.0762'], ['0', '0', '0'], 'base_footprint', 'camera_link'),    
    ]) 
          
    mini_tank_ = GroupAction(
        condition=IfCondition(mini_tank),
        actions=[
            generate_robot_node('mini_tank_robot.urdf','mini_tank'),
            generate_static_transform_publisher_node(['0.02799', '0.00005', '0.09602'], ['0', '0', '0'], 'base_footprint', 'laser'),
            generate_static_transform_publisher_node(['0.09673', '0.00012', '0.0777'], ['0', '0', '0'],'base_footprint', 'camera_link'), 
    ])            
    mini_4wd_ = GroupAction(
        condition=IfCondition(mini_4wd),
        actions=[
            generate_robot_node('mini_4wd_robot.urdf','mini_4wd'),
            generate_static_transform_publisher_node(['0.03163', '0.00009', '0.09292'], ['0', '0', '0'], 'base_footprint', 'laser'),
            generate_static_transform_publisher_node(['0.10709', '0.00032', '0.0767'], ['0', '0', '0'], 'base_footprint', 'camera_link'),
    ])            
    mini_diff_ = GroupAction(
        condition=IfCondition(mini_diff),
        actions=[
            generate_robot_node('mini_diff_robot.urdf','mini_diff'),
            generate_static_transform_publisher_node(['0.05134', '0', '0.09452'], ['0', '0', '0'], 'base_footprint', 'laser'),
            generate_static_transform_publisher_node(['0.11365', '0.00017', '0.0762'], ['0', '0', '0'], 'base_footprint', 'camera_link'),
    ])            
    brushless_senior_diff_ = GroupAction(
        condition=IfCondition(brushless_senior_diff),
        actions=[
            generate_robot_node('brushless_senior_diff_robot.urdf','brushless_senior_diff'),
            generate_static_transform_publisher_node(['0.10115', '0.00', '0.23618'], ['0', '0', '0'], 'base_footprint', 'laser'),
            generate_static_transform_publisher_node(['0.19024', '0.00024', '0.22022'], ['0', '0', '0'], 'base_footprint', 'camera_link'),        
    ])
    
    # Create the launch description and populate
    ld = LaunchDescription()

    # Set environment variables
    # Declare the launch options

    #ld.add_action(declare_use_composition_cmd)

    # Add the actions to launch all of the localiztion nodes
    ld.add_action(r3s_mec_)
    ld.add_action(r3s_4wd_)
      
    ld.add_action(mini_mec_)
    ld.add_action(mini_akm_)
    ld.add_action(mini_tank_)
    ld.add_action(mini_4wd_)
    ld.add_action(mini_diff_)
    ld.add_action(brushless_senior_diff_)


    return ld
