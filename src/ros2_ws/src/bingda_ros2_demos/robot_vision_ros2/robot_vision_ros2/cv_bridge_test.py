#!/usr/bin/python
# coding=gbk
# Copyright 2022 Bingda Robot.
# Developer: FuZhi, Liu (liu.fuzhi@bingda-robot.com)
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
  

import rclpy 
from rclpy.node import Node 
from sensor_msgs.msg import Image 
from cv_bridge import CvBridge
from rclpy.qos import QoSProfile
from rclpy.qos import qos_profile_sensor_data
from rclpy.qos import qos_profile_system_default
import cv2

class cv_bridge_test(Node):
    def __init__(self):

        # Initiate the Node class's constructor and give it a name
        super().__init__('cv_bridge_test')
        # Create the subscriber. This subscriber will receive an Image
        # from the video_frames topic. The queue size is 1 messages.
        self.image_subscription = self.create_subscription(Image, 'image_raw', self.listener_callback, qos_profile=qos_profile_sensor_data)      
        # Create the publisher. This publisher will publish an Image
        # to the video_frames topic. The queue size is 1 messages.
        self.cv_image_publisher_ = self.create_publisher(Image, 'cv_image', qos_profile=qos_profile_system_default)   
        # Used to convert between ROS and OpenCV images
        self.br = CvBridge()
        self.get_logger().info('cv_bridge_test Started')


    def listener_callback(self, data):
        """
        Callback function.
        """
        # Convert ROS Image message to OpenCV image
        cv_image  = self.br.imgmsg_to_cv2(data, "bgr8")

        # draw a circle in image
        (rows,cols,channels) = cv_image.shape
        if cols > 60 and rows > 60 :
            cv2.circle(cv_image, (60, 60), 30, (0,0,255), -1)

        ros_frame = self.br.cv2_to_imgmsg(cv_image, "bgr8")

        self.cv_image_publisher_.publish(ros_frame)

  
def main(args=None):
  
  # Initialize the rclpy library
  rclpy.init(args=args)
  
  # Create the node
  cv_bridge= cv_bridge_test()
  
  # Spin the node so the callback function is called.
  rclpy.spin(cv_bridge)
  
  # Destroy the node explicitly
  # (optional - otherwise it will be done automatically
  # when the garbage collector destroys the node object)
  cv_bridge.destroy_node()
  
  # Shutdown the ROS client library for Python
  rclpy.shutdown()
  
if __name__ == '__main__':
  main()