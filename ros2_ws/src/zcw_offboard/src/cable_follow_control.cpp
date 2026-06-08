#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/msg/position_target.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>
#include <vector>

using namespace std::chrono_literals;

struct Waypoint {
    double x, y, z;
};

class CableFollowControl : public rclcpp::Node {
public:
    CableFollowControl() : Node("cable_follow_control"), state_(false), armed_(false),
        wp_idx_(0), phase_(0) {
        // Tower and cable parameters
        declare_parameter("tower_x1", -30.0);
        declare_parameter("tower_x2", 30.0);
        declare_parameter("cable_y", 0.6);
        declare_parameter("attach_height", 25.0);
        declare_parameter("sag", 4.0);
        declare_parameter("num_wp", 20);
        declare_parameter("speed", 3.0);
        declare_parameter("proximity", 2.0);
        declare_parameter("fly_height", 30.0);

        double x1 = get_parameter("tower_x1").as_double();
        double x2 = get_parameter("tower_x2").as_double();
        cable_y_ = get_parameter("cable_y").as_double();
        att_h_ = get_parameter("attach_height").as_double();
        sag_ = get_parameter("sag").as_double();
        int num_wp = get_parameter("num_wp").as_int();
        speed_ = get_parameter("speed").as_double();
        prox_ = get_parameter("proximity").as_double();
        fly_height_ = get_parameter("fly_height").as_double();

        gen_cable_waypoints(x1, x2, num_wp);

        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            "mavros/state", 10,
            [this](const mavros_msgs::msg::State::SharedPtr msg) {
                state_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
            });

        local_pos_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
            "mavros/setpoint_position/local", 10);

        auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
        pos_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            "mavros/local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                pos_ = *msg;
            });

        arming_client_ = create_client<mavros_msgs::srv::CommandBool>("mavros/cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("mavros/set_mode");

        timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(1s, [this]() { send_commands(); });

        RCLCPP_INFO(get_logger(),
            "CableFollowControl: %d waypoints along cable y=%.1f, h=%.1f sag=%.1f",
            num_wp, cable_y_, att_h_, sag_);
    }

private:
    void gen_cable_waypoints(double x1, double x2, int n) {
        wps_.clear();
        double span = (x2 - x1) / 2.0;
        double cx = (x1 + x2) / 2.0;
        double inset = (x2 - x1) * 0.1;

        for (int i = 0; i < n; i++) {
            double frac = (double)i / (n - 1);
            double x = x1 + inset + frac * (x2 - x1 - 2 * inset);
            double x_rel = (x - cx) / span;
            double z = att_h_ - sag_ * (1 - x_rel * x_rel);
            wps_.push_back({x, cable_y_, z});
        }
    }

    void publish_setpoint() {
        if (wps_.empty()) return;

        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";

        if (phase_ == 0) {
            // Fly to first waypoint via approach height
            sp.pose.position.x = wps_[0].x;
            sp.pose.position.y = wps_[0].y;
            sp.pose.position.z = fly_height_;
        } else if (phase_ == 1) {
            // Descend to cable
            sp.pose.position.x = wps_[0].x;
            sp.pose.position.y = wps_[0].y;
            sp.pose.position.z = wps_[0].z;
        } else if (phase_ == 2) {
            sp.pose.position.x = wps_[wp_idx_].x;
            sp.pose.position.y = wps_[wp_idx_].y;
            sp.pose.position.z = wps_[wp_idx_].z;
        } else {
            return;
        }

        double dx, dy;
        if (phase_ < 2) {
            dx = wps_[0].x - sp.pose.position.x;
            dy = wps_[0].y - sp.pose.position.y;
        } else {
            int next = std::min(wp_idx_ + 1, (int)wps_.size() - 1);
            dx = wps_[next].x - sp.pose.position.x;
            dy = wps_[next].y - sp.pose.position.y;
        }
        double yaw = std::atan2(dy, dx);
        sp.pose.orientation.x = 0;
        sp.pose.orientation.y = 0;
        sp.pose.orientation.z = std::sin(yaw / 2);
        sp.pose.orientation.w = std::cos(yaw / 2);

        local_pos_pub_->publish(sp);
    }

    void send_commands() {
        if (!state_) {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Waiting for MAVROS...");
            return;
        }

        if (phase_ < 2) {
            if (!armed_) {
                RCLCPP_INFO(get_logger(), "Arming...");
                auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                req->value = true;
                arming_client_->async_send_request(req);
            } else if (mode_ != "OFFBOARD") {
                RCLCPP_INFO(get_logger(), "Setting OFFBOARD mode...");
                auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
                req->custom_mode = "OFFBOARD";
                set_mode_client_->async_send_request(req);
            } else if (phase_ == 0) {
                RCLCPP_INFO(get_logger(), "Flying to start of cable...");
                phase_ = 1;
                start_time_ = now().seconds();
            } else if (phase_ == 1 && dist_to_wp(wps_[0]) < prox_) {
                RCLCPP_INFO(get_logger(), "Reached cable start, following cable...");
                phase_ = 2;
                wp_idx_ = 0;
            }
        } else if (phase_ == 2) {
            double d = dist_to_wp(wps_[wp_idx_]);
            if (d < prox_ && wp_idx_ < (int)wps_.size() - 1) {
                wp_idx_++;
                RCLCPP_INFO(get_logger(), "Waypoint %d/%ld, distance=%.1f",
                    wp_idx_, wps_.size(), d);
            }
            if (wp_idx_ >= (int)wps_.size() - 1 && d < prox_) {
                RCLCPP_INFO(get_logger(), "Cable follow complete!");
                phase_ = 3;
            }
        }
    }

    double dist_to_wp(const Waypoint& wp) {
        return std::sqrt(
            std::pow(pos_.pose.position.x - wp.x, 2) +
            std::pow(pos_.pose.position.y - wp.y, 2) +
            std::pow(pos_.pose.position.z - wp.z, 2));
    }

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pos_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr local_pos_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::TimerBase::SharedPtr cmd_timer_;

    std::vector<Waypoint> wps_;
    geometry_msgs::msg::PoseStamped pos_;
    int wp_idx_;
    int phase_;
    double cable_y_, att_h_, sag_, speed_, prox_, fly_height_;
    double start_time_;
    bool state_, armed_;
    std::string mode_;
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CableFollowControl>());
    rclcpp::shutdown();
    return 0;
}
