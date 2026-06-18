import rclpy, time, signal, sys
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from geometry_msgs.msg import PoseStamped
from mavros_msgs.srv import SetMode, CommandBool

rclpy.init()
n = Node('hover_node')

BEST_EFFORT = QoSProfile(reliability=ReliabilityPolicy.BEST_EFFORT, history=HistoryPolicy.KEEP_LAST, depth=1)

sp_pub = n.create_publisher(PoseStamped, '/mavros/setpoint_position/local', BEST_EFFORT)
arm_cli = n.create_client(CommandBool, '/mavros/cmd/arming')
mode_cli = n.create_client(SetMode, '/mavros/set_mode')

running = True
def sig_handler(sig, frame):
    global running
    running = False
signal.signal(signal.SIGINT, sig_handler)
signal.signal(signal.SIGTERM, sig_handler)

arm_cli.wait_for_service(timeout_sec=10.0)
mode_cli.wait_for_service(timeout_sec=10.0)

pose = PoseStamped()
pose.header.frame_id = 'map'
pose.pose.position.x = 0.0
pose.pose.position.y = 0.0
pose.pose.position.z = 10.0
pose.pose.orientation.w = 1.0

# Stream setpoints 5s before OFFBOARD
for i in range(250):
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    time.sleep(0.02)

# Set OFFBOARD
future = mode_cli.call_async(SetMode.Request(custom_mode='OFFBOARD'))
rclpy.spin_until_future_complete(n, future, timeout_sec=5.0)
print(f"OFFBOARD: {future.result().mode_sent if future.done() else 'timeout'}", flush=True)

# Stream 1s then arm
for i in range(50):
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    time.sleep(0.02)

future = arm_cli.call_async(CommandBool.Request(value=True))
rclpy.spin_until_future_complete(n, future, timeout_sec=5.0)
r = future.result() if future.done() else None
print(f"ARM: success={r.success if r else 'timeout'}", flush=True)

# Continuous setpoint stream until interrupted
print("Hovering at 10m... (Ctrl+C to stop)", flush=True)
while running:
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    rclpy.spin_once(n, timeout_sec=0)
    time.sleep(0.02)

print("Landing...", flush=True)
future = mode_cli.call_async(SetMode.Request(custom_mode='AUTO.LAND'))
rclpy.spin_until_future_complete(n, future, timeout_sec=5.0)
rclpy.shutdown()
