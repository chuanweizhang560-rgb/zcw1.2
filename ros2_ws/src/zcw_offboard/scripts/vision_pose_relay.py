#!/usr/bin/env python3
"""vision_pose_relay: FAST-LIVO2 /aft_mapped_to_init → /mavros/vision_pose/pose"""
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import PoseStamped

class VisionPoseRelay(Node):
    def __init__(self):
        super().__init__('vision_pose_relay')
        self.pub = self.create_publisher(PoseStamped, '/mavros/vision_pose/pose', 10)
        self.sub = self.create_subscription(Odometry, '/aft_mapped_to_init', self.cb, 10)
        self.get_logger().info('vision_pose_relay started')

    def cb(self, msg):
        ps = PoseStamped()
        ps.header = msg.header
        ps.header.frame_id = 'map'
        ps.pose = msg.pose.pose
        self.pub.publish(ps)

def main(args=None):
    rclpy.init(args=args)
    node = VisionPoseRelay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    rclpy.shutdown()

if __name__ == '__main__':
    main()
