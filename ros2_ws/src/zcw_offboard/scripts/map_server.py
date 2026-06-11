#!/usr/bin/env python3
"""map_server: 增量地图服务器
订阅 FAST-LIVO2 /Laser_map, 维护全局体素占用网格
输出: nav_msgs/OccupancyGrid (2D投影) + 地图查询服务
"""
import rclpy, numpy as np, threading
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from nav_msgs.msg import OccupancyGrid
from std_srvs.srv import SetBool
import sensor_msgs_py.point_cloud2 as pc2

class MapServer(Node):
    def __init__(self):
        super().__init__('map_server')
        self.declare_parameter('resolution', 0.5)
        self.declare_parameter('map_width', 200)
        self.declare_parameter('map_height', 200)
        self.declare_parameter('origin_x', -50.0)
        self.declare_parameter('origin_y', -50.0)
        self.declare_parameter('max_z', 5.0)
        self.declare_parameter('min_z', -1.0)
        self.declare_parameter('prob_hit', 0.7)
        self.declare_parameter('prob_miss', 0.4)
        self.declare_parameter('occ_threshold', 0.6)

        self.res = self.get_parameter('resolution').value
        self.w = self.get_parameter('map_width').value
        self.h = self.get_parameter('map_height').value
        self.ox = self.get_parameter('origin_x').value
        self.oy = self.get_parameter('origin_y').value
        self.max_z = self.get_parameter('max_z').value
        self.min_z = self.get_parameter('min_z').value
        self.p_hit = self.get_parameter('prob_hit').value
        self.p_miss = self.get_parameter('prob_miss').value
        self.occ_th = self.get_parameter('occ_threshold').value

        self.log_odds = np.zeros((self.h, self.w), dtype=np.float32)
        self.lock = threading.Lock()
        self.update_count = 0

        self.sub = self.create_subscription(
            PointCloud2, '/cloud_registered', self.cb, 10)
        self.pub = self.create_publisher(OccupancyGrid, '/global_map', 10)
        self.srv = self.create_service(SetBool, '~/reset_map', self.reset_cb)

        self.timer = self.create_timer(1.0, self.publish_map)
        self.get_logger().info(
            f'map_server started: {self.w}x{self.h} x {self.res}m '
            f'origin=({self.ox},{self.oy}) z=[{self.min_z},{self.max_z}]'
            f' sub=/cloud_registered pub=/global_map')

    def world_to_grid(self, x, y):
        gx = int((x - self.ox) / self.res)
        gy = int((y - self.oy) / self.res)
        return gx, gy

    def cb(self, msg):
        try:
            pts = list(pc2.read_points(msg, field_names=('x', 'y', 'z'),
                                        skip_nans=True))
        except Exception as e:
            self.get_logger().warn(f'pc2 read failed: {e}')
            return
        if not pts:
            return
        pts = np.array(pts)
        mask = (pts[:, 2] >= self.min_z) & (pts[:, 2] <= self.max_z)
        pts = pts[mask]
        if pts.size == 0:
            return
        gx = ((pts[:, 0] - self.ox) / self.res).astype(int)
        gy = ((pts[:, 1] - self.oy) / self.res).astype(int)
        valid = (gx >= 0) & (gx < self.w) & (gy >= 0) & (gy < self.h)
        gx, gy = gx[valid], gy[valid]
        with self.lock:
            self.log_odds[gy, gx] += np.log(self.p_hit / (1 - self.p_hit))
            # simple decay for non-hit cells is handled by not doing anything
            self.update_count += 1
            if self.update_count % 10 == 0:
                self.get_logger().info(
                    f'Map update #{self.update_count}: +{len(gx)} cells')

    def publish_map(self):
        with self.lock:
            probs = 1 - 1 / (1 + np.exp(self.log_odds))
            data = np.where(probs > self.occ_th, 100,
                            np.where(probs < 0.2, 0, -1)).astype(np.int8)
        og = OccupancyGrid()
        og.header.stamp = self.get_clock().now().to_msg()
        og.header.frame_id = 'map'
        og.info.resolution = self.res
        og.info.width = self.w
        og.info.height = self.h
        og.info.origin.position.x = self.ox
        og.info.origin.position.y = self.oy
        og.info.origin.position.z = 0.0
        og.info.origin.orientation.w = 1.0
        og.data = data.ravel().tolist()
        self.pub.publish(og)

    def reset_cb(self, req, res):
        if req.data:
            with self.lock:
                self.log_odds.fill(0)
                self.update_count = 0
            self.get_logger().info('Map reset')
        res.success = True
        res.message = 'map reset' if req.data else 'no action'
        return res

def main(args=None):
    rclpy.init(args=args)
    node = MapServer()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
