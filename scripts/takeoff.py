import rclpy, time, sys
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from geometry_msgs.msg import PoseStamped
from mavros_msgs.srv import SetMode, CommandBool

rclpy.init()
n = Node('takeoff_node')

BEST_EFFORT = QoSProfile(reliability=ReliabilityPolicy.BEST_EFFORT, history=HistoryPolicy.KEEP_LAST, depth=1)

sp_pub = n.create_publisher(PoseStamped, '/mavros/setpoint_position/local', BEST_EFFORT)
arm_cli = n.create_client(CommandBool, '/mavros/cmd/arming')
mode_cli = n.create_client(SetMode, '/mavros/set_mode')

print("Waiting services...", flush=True)
arm_cli.wait_for_service(timeout_sec=10.0)
mode_cli.wait_for_service(timeout_sec=10.0)
print("Services ready", flush=True)

pose = PoseStamped()
pose.header.frame_id = 'map'
pose.pose.position.x = 0.0
pose.pose.position.y = 0.0
pose.pose.position.z = 10.0
pose.pose.orientation.w = 1.0

# Stream setpoints for 5 seconds BEFORE OFFBOARD (PX4 requires this)
print("Streaming setpoints 5s...", flush=True)
for i in range(250):
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    time.sleep(0.02)

# Set OFFBOARD mode
print("Setting OFFBOARD...", flush=True)
future = mode_cli.call_async(SetMode.Request(custom_mode='OFFBOARD'))
rclpy.spin_until_future_complete(n, future, timeout_sec=5.0)
if future.done():
    r = future.result()
    print(f"OFFBOARD: {r.mode_sent}", flush=True)
else:
    print("OFFBOARD timeout", flush=True)

# Keep streaming for 1s after OFFBOARD
for i in range(50):
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    time.sleep(0.02)

# Arm
print("Arming...", flush=True)
future = arm_cli.call_async(CommandBool.Request(value=True))
rclpy.spin_until_future_complete(n, future, timeout_sec=5.0)
if future.done():
    r = future.result()
    print(f"ARM: success={r.success} result={r.result}", flush=True)
else:
    print("ARM timeout", flush=True)

# Stream setpoints for 30 seconds (continuous - required by OFFBOARD)
print("Streaming 30s...", flush=True)
for i in range(1500):
    pose.header.stamp = n.get_clock().now().to_msg()
    sp_pub.publish(pose)
    rclpy.spin_once(n, timeout_sec=0)
    time.sleep(0.02)

print("Done", flush=True)
rclpy.shutdown()
