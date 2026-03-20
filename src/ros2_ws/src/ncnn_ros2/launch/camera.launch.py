from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    # 声明参数
    video_device = LaunchConfiguration('video_device', default='/dev/video0')
    image_width = LaunchConfiguration('image_width', default='640')
    image_height = LaunchConfiguration('image_height', default='480')
    pixel_format = LaunchConfiguration('pixel_format', default='yuyv')
    framerate = LaunchConfiguration('framerate', default='30.0')
    io_method = LaunchConfiguration('io_method', default='mmap')

    # 创建节点
    usb_cam_node = Node(
        package='usb_cam',
        executable='usb_cam_node_exe',
        name='usb_cam',
        output='screen',
        parameters=[{
            'video_device': video_device,
            'image_width': image_width,
            'image_height': image_height,
            'pixel_format': pixel_format,
            'framerate': framerate,
            'io_method': io_method,
        }]
    )

    return LaunchDescription([
        usb_cam_node
    ])
