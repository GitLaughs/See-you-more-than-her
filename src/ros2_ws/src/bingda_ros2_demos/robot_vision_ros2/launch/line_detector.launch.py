from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    # 声明参数
    test_mode = LaunchConfiguration('test_mode', default='False')
    h_lower = LaunchConfiguration('h_lower', default='110')
    s_lower = LaunchConfiguration('s_lower', default='50')
    v_lower = LaunchConfiguration('v_lower', default='50')
    h_upper = LaunchConfiguration('h_upper', default='130')
    s_upper = LaunchConfiguration('s_upper', default='255')
    v_upper = LaunchConfiguration('v_upper', default='255')
    
    # 创建节点
    line_detector_node = Node(
        package='robot_vision_ros2',
        executable='line_detector',
        name='line_detector',
        output='screen',
        parameters=[{
            'test_mode': test_mode,
            'h_lower': h_lower,
            's_lower': s_lower,
            'v_lower': v_lower,
            'h_upper': h_upper,
            's_upper': s_upper,
            'v_upper': v_upper,
        }]
    )

    return LaunchDescription([
        line_detector_node
    ])
