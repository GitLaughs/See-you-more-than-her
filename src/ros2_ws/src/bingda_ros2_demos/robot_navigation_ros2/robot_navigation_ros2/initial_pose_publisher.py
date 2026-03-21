import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseWithCovarianceStamped
from nav_msgs.msg import Odometry

class InitialPosePublisher(Node):

    def __init__(self):
        super().__init__('initial_pose_publisher')
        self.publisher_ = self.create_publisher(PoseWithCovarianceStamped, 'initialpose', 10)
        self.amcl_subscription = self.create_subscription(
            PoseWithCovarianceStamped,
            '/amcl_pose',  # AMCL 发布位姿的主题
            self.amcl_pose_callback,
            10
        )
        self.initial_pose_published = False
        self.timer = self.create_timer(1.0, self.publish_initial_pose)  # 每秒发布一次，直到AMCL接受

    def publish_initial_pose(self):
        if not self.initial_pose_published:
            initial_pose = PoseWithCovarianceStamped()
            initial_pose.header.frame_id = "map"
            initial_pose.pose.pose.position.x = 0.0  # 设置初始X坐标
            initial_pose.pose.pose.position.y = 0.0  # 设置初始Y坐标
            initial_pose.pose.pose.position.z = 0.0
            initial_pose.pose.pose.orientation.z = 0.0  # 初始朝向的四元数
            initial_pose.pose.pose.orientation.w = 1.0
            self.publisher_.publish(initial_pose)
            self.get_logger().info('Initial pose published')

    def amcl_pose_callback(self, msg):
        # 收到 AMCL 的反馈，表明定位已经初始化，停止发布
        if self.initial_pose_published == False:
            self.get_logger().info('AMCL pose received, stopping initial pose publishing.')
        self.initial_pose_published = True
        self.timer.cancel()  # 停止定时器

def main(args=None):
    rclpy.init(args=args)
    node = InitialPosePublisher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
