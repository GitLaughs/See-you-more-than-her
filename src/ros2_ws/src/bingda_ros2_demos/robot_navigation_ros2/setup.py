import os
from glob import glob
from setuptools import setup

package_name = 'robot_navigation_ros2'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Include all launch files. This is the most important line here!
        (os.path.join('share', package_name, 'launch'), glob('launch/*.*')),  
        (os.path.join('share', package_name, 'launch'), glob('launch/*/*.*')),
        (os.path.join('share', package_name, 'config'), glob('config/*.*')),
        (os.path.join('share', package_name, 'map'), glob('map/*.*')),
        (os.path.join('share', package_name, 'param'), glob('param/*.*')),
        (os.path.join('share', package_name, 'params'), glob('params/*.*')),
        (os.path.join('share', package_name, 'rviz'), glob('rviz/*.*'))
        # (os.path.join('share', package_name, 'script'), glob('script/*.*'))
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='bingda',
    maintainer_email='bingda@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'navigation_test = robot_navigation_ros2.navigation_test:main',
            'initial_pose_publisher = robot_navigation_ros2.initial_pose_publisher:main'
        ],
    },
)
