import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from std_msgs.msg import Header
from object_information_msgs_ros2.msg import Object  # 自定义消息
from geometry_msgs.msg import Pose, Vector3
from cv_bridge import CvBridge
import cv2
import time
from .yolov8 import YoloV8s
from .visual import draw_detection_objects

class ImageDetectionNode(Node):
    def __init__(self):
        super().__init__('image_detection_node')
        
        # Declare parameters
        self.declare_parameter("load_path", "/home/bingda/ncnn-assets/models")
        self.declare_parameter("file_name", "yolov8s")
        self.declare_parameter("target_size", 640)
        self.declare_parameter("prob_threshold", 0.25)
        self.declare_parameter("nms_threshold", 0.45)
        self.declare_parameter("num_threads", 4)
        self.declare_parameter("use_gpu", False)

        # Get parameters
        load_path = self.get_parameter("load_path").get_parameter_value().string_value
        file_name = self.get_parameter("file_name").get_parameter_value().string_value
        target_size = self.get_parameter("target_size").get_parameter_value().integer_value
        prob_threshold = self.get_parameter("prob_threshold").get_parameter_value().double_value
        nms_threshold = self.get_parameter("nms_threshold").get_parameter_value().double_value
        num_threads = self.get_parameter("num_threads").get_parameter_value().integer_value
        use_gpu = self.get_parameter("use_gpu").get_parameter_value().bool_value

        # Initialize a YoloV8s detector with parameters from ROS
        self.net = YoloV8s(
            load_path=load_path,
            file_name=file_name,
            target_size=target_size,
            prob_threshold=prob_threshold,
            nms_threshold=nms_threshold,
            num_threads=num_threads,
            use_gpu=use_gpu,
        )
        
        # Initialize a CvBridge to convert ROS images to OpenCV images and vice versa
        self.bridge = CvBridge()

        # Subscribe to the /image_raw topic
        self.subscription = self.create_subscription(
            Image,
            '/image_raw',
            self.image_callback,
            1
        )

        # Publisher for the processed image with detections
        self.publisher_image = self.create_publisher(
            Image,
            '/ncnn_image',
            1
        )

        # Publisher for object detection results
        self.publisher_objects = self.create_publisher(
            Object,
            '/objects',
            1
        )

        # Variables to calculate detection frequency
        self.last_time = time.time()
        self.frequency = 0.0
        self.detect_sequence = 0  # Track detection sequence number

    def image_callback(self, msg):
        # Get the current time and calculate the frequency
        current_time = time.time()
        delta_time = current_time - self.last_time
        if delta_time > 0:
            self.frequency = 1.0 / delta_time
        self.last_time = current_time

        # Convert ROS Image message to OpenCV image
        cv_image = self.bridge.imgmsg_to_cv2(msg, "bgr8")

        # Perform object detection
        objects = self.net(cv_image)

        # Draw the detection results on the image
        detection_image = draw_detection_objects(cv_image, self.net.class_names, objects)

        # Display frequency in the top-left corner
        cv2.putText(
            detection_image,
            f"FPS: {self.frequency:.2f} Hz",
            (10, 30),  # Position (x, y)
            cv2.FONT_HERSHEY_SIMPLEX,  # Font type
            1,  # Font scale
            (0, 255, 0),  # Color (green)
            2  # Thickness
        )

        # Convert OpenCV image back to ROS Image message
        ros_image = self.bridge.cv2_to_imgmsg(detection_image, encoding="bgr8")

        # Publish the image with detections and frequency text
        self.publisher_image.publish(ros_image)

        # Increment detection sequence number
        self.detect_sequence += 1

        # Publish detection results as Object messages
        for i, obj in enumerate(objects):
            object_msg = Object()
            object_msg.header = Header()
            object_msg.header.stamp = self.get_clock().now().to_msg()
            object_msg.header.frame_id = msg.header.frame_id  # Use the frame_id from the image message
            object_msg.detect_sequence = self.detect_sequence
            object_msg.object_total = len(objects)
            object_msg.object_sequence = i + 1  # Start object sequence from 1
            object_msg.label = self.net.class_names[int(obj.label)]
            object_msg.probability = float(obj.prob)

            # Convert bounding box to position and size
            # Access bounding box attributes
            x = obj.rect.x
            y = obj.rect.y
            w = obj.rect.w
            h = obj.rect.h
            object_msg.position = Pose()
            object_msg.position.position.x = x + w / 2.0  # Center x
            object_msg.position.position.y = y + h / 2.0  # Center y
            object_msg.position.position.z = 0.0  # Assuming 2D detection, so z=0

            object_msg.size = Vector3()
            object_msg.size.x = w  # Width of the bounding box
            object_msg.size.y = h  # Height of the bounding box
            object_msg.size.z = 0.0  # Assuming 2D detection, so depth=0

            # Publish the object information
            self.publisher_objects.publish(object_msg)

def main(args=None):
    rclpy.init(args=args)

    # Initialize and spin the node
    node = ImageDetectionNode()
    rclpy.spin(node)

    # Shutdown and cleanup
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
