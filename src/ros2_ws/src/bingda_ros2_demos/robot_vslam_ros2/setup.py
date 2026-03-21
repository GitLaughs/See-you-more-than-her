from setuptools import find_packages, setup
import os
from glob import glob
from pathlib import Path

package_name = 'robot_vslam_ros2'

def package_files(directory):
    """
    Recursively collect all files in the given directory.
    """
    paths = []
    for path in Path(directory).rglob('*'):
        if path.is_file():
            paths.append((os.path.join('share', package_name, directory,path.parent.relative_to(directory)), [str(path)]))
    return paths

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Include all launch files. This is the most important line here!
        (os.path.join('share', package_name, 'launch'), glob('launch/*.*')),  
        (os.path.join('share', package_name, 'launch'), glob('launch/*/*.*')),
         *package_files('config'),
        # (os.path.join('share', package_name, 'config'), glob('config/**/*.*')),
        # (os.path.join('share', package_name, 'config'), glob('config/*.*')),
        (os.path.join('share', package_name, 'rviz'), glob('rviz/*.*'))
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
        ],
    },
)
