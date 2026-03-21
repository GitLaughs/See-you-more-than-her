from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'ncnn_ros2'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name,'launch'), glob('launch/*launch.py')),
        (os.path.join('share', package_name,'models'), glob('models/*.*')),
    ],
    install_requires=['setuptools','object_information_msgs'],
    zip_safe=True,
    maintainer='bingda',
    maintainer_email='bingda@todo.todo',
    description='TODO: Package description',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'ncnn_ros2_test = ncnn_ros2.ncnn_ros2_test:main',
            'yolov8_ros = ncnn_ros2.yolov8_ros:main',
            'yolov7_ros = ncnn_ros2.yolov7_ros:main',
            'yolov5_ros = ncnn_ros2.yolov5_ros:main', 
            'yolov3_ros = ncnn_ros2.yolov3_ros:main', 
            'yolov10_ros = ncnn_ros2.yolov10_ros:main',           
        ],
    },
)
