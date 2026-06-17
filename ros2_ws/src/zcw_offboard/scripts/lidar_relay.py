#!/usr/bin/env python3
# lidar_relay: 修复 CPU ray velodyne PointCloud2 字段类型错乱
# CPU ray bug: 字段声明为 FLOAT64 但实际数据是 FLOAT32 布局
# 修正后输出: x(F32) y(F32) z(F32) intensity(F32) ring(U16, 标识激光线号)
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2, PointField
import struct

class LidarRelay(Node):
    def __init__(self):
        super().__init__('lidar_relay')
        self.pub = self.create_publisher(PointCloud2, '/velodyne/points_raw', 10)
        self.sub = self.create_subscription(
            PointCloud2, '/velodyne_laser_plugin/out', self.cb, 10)
        self.get_logger().info('lidar_relay node started')

    def cb(self, msg):
        raw = msg.data.tobytes() if hasattr(msg.data, 'tobytes') else bytes(msg.data)
        npts = msg.width * msg.height
        step = msg.point_step  # 22 bytes
        scan_duration = 0.1  # VLP-16 10Hz, 0.1s per full scan

        # new_step = 22 bytes: x(F32) y(F32) z(F32) intensity(F32) ring(U16) time(F32)
        new_step = 22
        out = bytearray(npts * new_step)
        for i in range(npts):
            off = i * step
            if off + 22 <= len(raw):
                x, y, z, intensity = struct.unpack_from('<ffff', raw, off)
                ring_data = struct.unpack_from('<I', raw, off + 16)[0]
                # Gazebo VLP-16 plugin time field is always 0 → compute sweep progress
                time_data = (i / npts) * scan_duration  # linear 0~0.1s across scan
                struct.pack_into('<ffffHf', out, i * new_step,
                                 x, y, z, intensity, ring_data & 0xFFFF, time_data)

        out_msg = PointCloud2()
        out_msg.header = msg.header
        out_msg.header.stamp = msg.header.stamp
        out_msg.width = npts
        out_msg.height = 1
        out_msg.point_step = new_step
        out_msg.row_step = new_step * npts
        out_msg.is_bigendian = False
        out_msg.is_dense = True
        out_msg.data = bytes(out)
        f = PointField
        out_msg.fields = [
            PointField(name='x', offset=0, datatype=f.FLOAT32, count=1),
            PointField(name='y', offset=4, datatype=f.FLOAT32, count=1),
            PointField(name='z', offset=8, datatype=f.FLOAT32, count=1),
            PointField(name='intensity', offset=12, datatype=f.FLOAT32, count=1),
            PointField(name='ring', offset=16, datatype=f.UINT16, count=1),
            PointField(name='time', offset=18, datatype=f.FLOAT32, count=1),
        ]
        self.get_logger().info(f'Publishing {len(out_msg.fields)} fields: {[field.name for field in out_msg.fields]}')
        self.pub.publish(out_msg)

def main(args=None):
    rclpy.init(args=args)
    node = LidarRelay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    rclpy.shutdown()

if __name__ == '__main__':
    main()
