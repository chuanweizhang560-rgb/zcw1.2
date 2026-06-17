#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
import tf2_ros
from geometry_msgs.msg import TransformStamped
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

class LocalPoseRelay(Node):
    def __init__(self):
        super().__init__('local_pose_relay')
        qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.sub = self.create_subscription(PoseStamped, '/mavros/local_position/pose', self.cb, qos)
        self.pub = self.create_publisher(Odometry, '/af_mapped_to_init', 10)
        self.br = tf2_ros.TransformBroadcaster(self)
        self.get_logger().info('local_pose_relay: /mavros/local_position/pose → /af_mapped_to_init')

    def cb(self, msg):
        odom = Odometry()
        odom.header = msg.header
        odom.header.frame_id = 'map'
        odom.child_frame_id = 'base_link'
        odom.pose.pose = msg.pose
        self.pub.publish(odom)

        t = TransformStamped()
        t.header = msg.header
        t.header.frame_id = 'map'
        t.child_frame_id = 'base_link'
        t.transform.translation.x = msg.pose.position.x
        t.transform.translation.y = msg.pose.position.y
        t.transform.translation.z = msg.pose.position.z
        t.transform.rotation = msg.pose.orientation
        self.br.sendTransform(t)

def main(args=None):
    rclpy.init(args=args)
    node = LocalPoseRelay()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
