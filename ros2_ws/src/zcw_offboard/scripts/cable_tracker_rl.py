#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped
from mavros_msgs.msg import State
from mavros_msgs.srv import CommandBool, SetMode
from stable_baselines3 import PPO
import numpy as np
import os


def cable_point(t, x1=-30.0, x2=30.0, cy=0.6, ah=25.0, sag=4.0):
    x = x1 + t * (x2 - x1)
    cx = (x1 + x2) / 2.0
    span = (x2 - x1) / 2.0
    xr = (x - cx) / span
    z = ah - sag * (1 - xr * xr)
    return np.array([x, cy, z], dtype=np.float32)


class CableTrackerRL(Node):
    def __init__(self):
        super().__init__("cable_tracker_rl")
        self.declare_parameter("tower_x1", -30.0)
        self.declare_parameter("tower_x2", 30.0)
        self.declare_parameter("model_path", "")
        self.declare_parameter("mavros_ns", "")

        self.x1 = self.get_parameter("tower_x1").value
        self.x2 = self.get_parameter("tower_x2").value
        model_path = self.get_parameter("model_path").value
        ns = self.get_parameter("mavros_ns").value
        pref = f"{ns}/" if ns else ""

        self.pos = np.array([0.0, 0.0, 0.0])
        self.connected = False
        self.armed = False
        self.mode = ""
        self.t_progress = 0.0
        self.last_cmd = 0.0

        self.model = None
        if model_path and os.path.exists(model_path):
            self.model = PPO.load(model_path)
            self.get_logger().info(f"PPO model loaded: {model_path}")
        else:
            self.get_logger().warn("No model, fallback only")

        self.state_sub = self.create_subscription(State, pref + "state", self.state_cb, 10)
        self.pos_sub = self.create_subscription(
            PoseStamped, pref + "local_position/pose", self.pos_cb, 10)

        self.sp_pub = self.create_publisher(PoseStamped, pref + "setpoint_position/local", 10)
        self.arm_cli = self.create_client(CommandBool, pref + "cmd/arming")
        self.mode_cli = self.create_client(SetMode, pref + "set_mode")

        self.create_timer(0.05, self.pub_sp)    # 20Hz setpoints
        self.create_timer(0.1, self.update)       # 10Hz state machine

    def state_cb(self, msg):
        self.connected = msg.connected
        self.armed = msg.armed
        self.mode = msg.mode

    def pos_cb(self, msg):
        self.pos = np.array([msg.pose.position.x, msg.pose.position.y, msg.pose.position.z])

    def pub_sp(self):
        if not self.connected:
            return
        sp = PoseStamped()
        sp.header.stamp = self.get_clock().now().to_msg()
        sp.header.frame_id = "map"

        wp = cable_point(self.t_progress, self.x1, self.x2)
        use_rl = self.model is not None and self.armed and self.mode == "OFFBOARD"

        if use_rl:
            err = self.pos - wp
            obs = np.array([[self.t_progress, err[0], err[1], err[2]]], dtype=np.float32)
            action, _ = self.model.predict(obs, deterministic=True)
            dt = np.clip(action[0][0], -1.0, 1.0) * 0.03
            dy = np.clip(action[0][1], -1.0, 1.0) * 2.0
            dz = np.clip(action[0][2], -1.0, 1.0) * 2.0
            self.t_progress = np.clip(self.t_progress + dt + 0.015, 0.0, 1.0)
            wp = cable_point(self.t_progress, self.x1, self.x2)
            sp.pose.position.x = float(wp[0] + dy)
            sp.pose.position.y = float(wp[1] + dy)
            sp.pose.position.z = float(wp[2] + dz + 1.0)
        else:
            self.t_progress = np.clip(self.t_progress + 0.009, 0.0, 1.0)
            wp = cable_point(self.t_progress, self.x1, self.x2)
            sp.pose.position.x = float(wp[0])
            sp.pose.position.y = float(wp[1])
            sp.pose.position.z = float(wp[2] + 2.0)

        sp.pose.orientation.w = 1.0
        self.sp_pub.publish(sp)

    def update(self):
        if not self.connected:
            return
        now_s = self.get_clock().now().nanoseconds / 1e9
        if now_s - self.last_cmd < 1.0:
            return
        self.last_cmd = now_s

        if self.mode != "OFFBOARD":
            req = SetMode.Request()
            req.custom_mode = "OFFBOARD"
            self.mode_cli.call_async(req)
            return
        if not self.armed:
            req = CommandBool.Request()
            req.value = True
            self.arm_cli.call_async(req)
            return

        wp = cable_point(self.t_progress, self.x1, self.x2)
        err = float(np.linalg.norm(self.pos - wp))
        self.get_logger().info(
            f"RL t={self.t_progress:.2f} err={err:.2f} pos=({self.pos[0]:.1f},{self.pos[1]:.1f},{self.pos[2]:.1f})",
            throttle_duration_sec=5)

        if self.t_progress >= 0.99:
            self.get_logger().info("RL tracker completed cable")
            req = CommandBool.Request()
            req.value = False
            self.arm_cli.call_async(req)


def main():
    rclpy.init()
    rclpy.spin(CableTrackerRL())
    rclpy.shutdown()
