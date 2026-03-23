import os 
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction,IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration,PathJoinSubstitution
from launch.conditions import IfCondition,UnlessCondition
from launch_ros.actions import Node
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory


#def launch(launch_descriptor, argv):
def generate_launch_description():
    # Declare the launch arguments
    use_wheeltec_imu_declare = DeclareLaunchArgument(
        'use_wheeltec_imu',
        default_value='false',  
        description='If true, use wheeltec_imu'
    )
    declare_use_imu = LaunchConfiguration('use_wheeltec_imu')

    

    # Extract common parameters
    common_params = {
        'usart_port_name': '/dev/wheeltec_controller',
        'serial_baud_rate': 115200,
        'robot_frame_id': 'base_footprint',
        'odom_frame_id': 'odom_combined',
        'cmd_vel': 'cmd_vel',
        'akm_cmd_vel': 'none',
        'product_number': 0,
        'odom_x_scale': 1.0,
        'odom_y_scale': 1.0,
        'odom_z_scale_positive': 1.0,
        'odom_z_scale_negative': 1.0
    }
    
    remappings=[('imu/data_raw', 'imu/data_board')]

    turn_on_robot_use_imu = Node(
        condition=UnlessCondition(declare_use_imu),
        package='turn_on_wheeltec_robot', 
        executable='wheeltec_robot_node', 
        output='screen',
        parameters=[common_params],
    )

    turn_on_robot = GroupAction(
        condition=IfCondition(declare_use_imu),
        actions=[
            Node(
                package='turn_on_wheeltec_robot', 
                executable='wheeltec_robot_node', 
                output='screen',
                parameters=[common_params],
                remappings=remappings,),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(get_package_share_directory('yesense_std_ros2'), 
                                'launch', 'yesense_node.launch.py'))),
        ]
    )


    ld = LaunchDescription()
    ld.add_action(use_wheeltec_imu_declare)  # 添加声明到ld
    ld.add_action(turn_on_robot_use_imu)
    ld.add_action(turn_on_robot)

    return ld
