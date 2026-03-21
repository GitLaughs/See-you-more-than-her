from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # 获取 ncnn_ros2 功能包的共享目录路径，并设置默认的模型路径
    default_model_path = os.path.join(get_package_share_directory('ncnn_ros2'), 'models')

    # Declare parameters with default values
    load_path_arg = DeclareLaunchArgument(
        'load_path', default_value=default_model_path,
        description='Path to the model files in ncnn_ros2 package'
    )
    file_name_arg = DeclareLaunchArgument(
        'file_name', default_value='yolov5s',
        description='Model file name'
    )
    target_size_arg = DeclareLaunchArgument(
        'target_size', default_value='640',
        description='Target size for input images'
    )
    prob_threshold_arg = DeclareLaunchArgument(
        'prob_threshold', default_value='0.25',
        description='Probability threshold for detection'
    )
    nms_threshold_arg = DeclareLaunchArgument(
        'nms_threshold', default_value='0.45',
        description='Non-maximum suppression threshold'
    )
    num_threads_arg = DeclareLaunchArgument(
        'num_threads', default_value='4',
        description='Number of threads for the model'
    )
    use_gpu_arg = DeclareLaunchArgument(
        'use_gpu', default_value='False',
        description='Flag to use GPU for detection'
    )

    # Define the node
    yolov8_node = Node(
        package='ncnn_ros2',
        executable='yolov5_ros',
        name='yolov5_ros',
        output='screen',
        parameters=[
            {
                'load_path': LaunchConfiguration('load_path'),
                'file_name': LaunchConfiguration('file_name'),
                'target_size': LaunchConfiguration('target_size'),
                'prob_threshold': LaunchConfiguration('prob_threshold'),
                'nms_threshold': LaunchConfiguration('nms_threshold'),
                'num_threads': LaunchConfiguration('num_threads'),
                'use_gpu': LaunchConfiguration('use_gpu')
            }
        ]
    )

    return LaunchDescription([
        load_path_arg,
        file_name_arg,
        target_size_arg,
        prob_threshold_arg,
        nms_threshold_arg,
        num_threads_arg,
        use_gpu_arg,
        yolov8_node
    ])
