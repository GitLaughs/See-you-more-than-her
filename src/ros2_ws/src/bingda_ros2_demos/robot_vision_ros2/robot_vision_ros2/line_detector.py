import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter
from sensor_msgs.msg import Image
import cv2
from cv_bridge import CvBridge
import numpy as np
from geometry_msgs.msg import Twist

class LineFollowNode(Node):
    def __init__(self):
        super().__init__('line_follow')
        
        # Declare parameters (these can be set dynamically)
        self.declare_parameter('input_topic', '/image_raw')
        self.declare_parameter('test_mode', False)
        self.declare_parameter('h_lower', 110)
        self.declare_parameter('s_lower', 50)
        self.declare_parameter('v_lower', 50)
        self.declare_parameter('h_upper', 130)
        self.declare_parameter('s_upper', 255)
        self.declare_parameter('v_upper', 255)

        # Initialize parameters with their initial values
        self.input_topic = self.get_parameter('input_topic').value
        self.test_mode = self.get_parameter('test_mode').value
        self.h_lower = self.get_parameter('h_lower').value
        self.s_lower = self.get_parameter('s_lower').value
        self.v_lower = self.get_parameter('v_lower').value
        self.h_upper = self.get_parameter('h_upper').value
        self.s_upper = self.get_parameter('s_upper').value
        self.v_upper = self.get_parameter('v_upper').value
        self.get_logger().info(f'Waiting Image Topic: {self.input_topic}')
        # Declare the publishers and subscribers
        self.bridge = CvBridge()
        self.image_sub = self.create_subscription(Image, self.input_topic, self.callback, 10)
        self.mask_pub = self.create_publisher(Image, '/mask_image', 10)
        self.result_pub = self.create_publisher(Image, '/result_image', 10)
        self.pub_cmd = self.create_publisher(Twist, 'cmd_vel', 10)

        # Add the parameter callback to handle dynamic updates
        self.add_on_set_parameters_callback(self.parameter_update_callback)

        # Store initial values
        self.center_point = 0

        self.get_image_topic = False

    def parameter_update_callback(self, params):
        # Handle parameter changes
        for param in params:
            if param.name == 'test_mode':
                self.test_mode = param.value
                self.get_logger().info(f'test_mode set to: {self.test_mode}')
            elif param.name == 'h_lower':
                self.h_lower = param.value
                self.get_logger().info(f'h_lower set to: {self.h_lower}')
            elif param.name == 's_lower':
                self.s_lower = param.value
                self.get_logger().info(f's_lower set to: {self.s_lower}')
            elif param.name == 'v_lower':
                self.v_lower = param.value
                self.get_logger().info(f'v_lower set to: {self.v_lower}')
            elif param.name == 'h_upper':
                self.h_upper = param.value
                self.get_logger().info(f'h_upper set to: {self.h_upper}')
            elif param.name == 's_upper':
                self.s_upper = param.value
                self.get_logger().info(f's_upper set to: {self.s_upper}')
            elif param.name == 'v_upper':
                self.v_upper = param.value
                self.get_logger().info(f'v_upper set to: {self.v_upper}')

        # Return a successful SetParametersResult
        return rclpy.node.SetParametersResult(successful=True)

    def callback(self, data):
        try:
            cv_image = self.bridge.imgmsg_to_cv2(data, "bgr8")
        except Exception as e:
            self.get_logger().error(f'Failed to convert image: {e}')
            return
        if self.get_image_topic == False:
            self.get_image_topic = True
            self.get_logger().info(f'Get Image Topic: {self.input_topic}')
            self.get_logger().info(f'Start Line Follow')

        # Convert to HSV
        hsv_image = cv2.cvtColor(cv_image, cv2.COLOR_BGR2HSV)
        line_lower = np.array([self.h_lower, self.s_lower, self.v_lower])
        line_upper = np.array([self.h_upper, self.s_upper, self.v_upper])

        mask = cv2.inRange(hsv_image, line_lower, line_upper)
        kernel = np.ones((9, 9), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

        # Test mode: output the center point HSV value with debugging info
        res = cv_image
        if self.test_mode:
            cv2.circle(res, (int(hsv_image.shape[1] / 2), int(hsv_image.shape[0] * 3 / 4)), 5, (0, 0, 255), 1)
            cv2.line(res, (int(hsv_image.shape[1] / 2 - 10), int(hsv_image.shape[0] * 3 / 4)),
                     (int(hsv_image.shape[1] / 2 + 10), int(hsv_image.shape[0] * 3 / 4)), (0, 0, 255), 1)
            cv2.line(res, (int(hsv_image.shape[1] / 2), int(hsv_image.shape[0] * 3 / 4 - 10)),
                     (int(hsv_image.shape[1] / 2), int(hsv_image.shape[0] * 3 / 4 + 10)), (0, 0, 255), 1)
            self.get_logger().info(f"Point HSV Value is {hsv_image[int(hsv_image.shape[0] * 3 / 4), int(hsv_image.shape[1] / 2)]}")
        else:
            # Normal mode: add mask to the original image
            for i in range(-60, 100, 20):
                point = np.nonzero(mask[int(mask.shape[0] / 2 + i)])
                if len(point[0]) > 10:
                    self.center_point = int(np.mean(point))
                    cv2.circle(res, (self.center_point, int(hsv_image.shape[0] / 2 + i)), 5, (0, 0, 255), 5)
                    break

        # Publish processed images
        try:
            img_msg = self.bridge.cv2_to_imgmsg(res, encoding="bgr8")
            img_msg.header.stamp = self.get_clock().now().to_msg()
            self.result_pub.publish(img_msg)

            mask_msg = self.bridge.cv2_to_imgmsg(mask, encoding="passthrough")
            mask_msg.header.stamp = self.get_clock().now().to_msg()
            self.mask_pub.publish(mask_msg)
            
        except Exception as e:
            self.get_logger().error(f'Failed to publish image: {e}')

        # Calculate twist if center point is found
        if self.center_point:
            self.twist_calculate(hsv_image.shape[1] / 2, self.center_point)
        self.center_point = 0

    def twist_calculate(self, width, center):
        center = float(center)
        twist = Twist()
        twist.linear.x = 0.0
        twist.angular.z = ((width - center) / width) / 2.0
        if abs(twist.angular.z) < 0.2:
            twist.linear.x = 0.2 - twist.angular.z / 2.0
        else:
            twist.linear.x = 0.1
        self.pub_cmd.publish(twist)


def main(args=None):
    rclpy.init(args=args)
    node = LineFollowNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
