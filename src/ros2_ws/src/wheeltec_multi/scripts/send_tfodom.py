#!/usr/bin/env python3
# coding=utf-8
# 主车发送主车坐标以及主车速度给从车

import math
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
import tf_transformations
import time
import socket
import sys
import struct
import tf2_ros
from numpy import array
from rclpy.executors import MultiThreadedExecutor

from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry  


class sendtf(Node):
	def __init__(self):
		super().__init__('send_tfodom')
		qos = QoSProfile(depth=10)
		self.buf = tf2_ros.Buffer()
		self.listener = tf2_ros.TransformListener(self.buf, self)
		self.soc=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
		self.soc.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)#udp zhu播
		self.network = "224.0.0.7"
		self.soc.setsockopt(socket.IPPROTO_IP,socket.IP_ADD_MEMBERSHIP,socket.inet_aton(self.network)+socket.inet_aton("0.0.0.0"))
		self.odom_vx = 0.0
		self.odom_vy = 0.0
		self.odom_az = 0.0
		self.create_subscription(Odometry,'odom', self.odom_callback , qos)
		self.timer = self.create_timer(0.1,self.on_timer)
		
	def on_timer(self):
		try:
			now  = self.get_clock().now()
			trans = self.buf.lookup_transform("map","base_link",now,rclpy.duration.Duration(seconds=1.0))
		except Exception as e:
			print(e)
			return
		(roll,pitch,yaw) = tf_transformations.euler_from_quaternion([trans.transform.rotation.x,trans.transform.rotation.y,trans.transform.rotation.z,trans.transform.rotation.w])
		send_data = struct.pack("ffffff",trans.transform.translation.x,trans.transform.translation.y,yaw,self.odom_vx,self.odom_vy,self.odom_az)
		self.soc.sendto(send_data, (self.network,10000))
		print(trans.transform.translation.x,trans.transform.translation.y,yaw,self.odom_vx,self.odom_vy,self.odom_az)
		
	def odom_callback(self,msg):
		self.odom_vx = msg.twist.twist.linear.x
		self.odom_vy = msg.twist.twist.linear.y
		self.odom_az = msg.twist.twist.angular.z
		#print(self.odom_vx,self.odom_vy,self.odom_az)


if __name__ == '__main__':
	rclpy.init()
	node = sendtf()
	executor = MultiThreadedExecutor()
	executor.add_node(node)
	try:
		executor.spin()
	except KeyboardInterrupt:
		pass
	executor.shutdown()
	rclpy.shutdown()
