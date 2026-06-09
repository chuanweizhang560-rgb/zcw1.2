#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>
#include <vector>

using namespace std::chrono_literals;

struct Vec3 { double x=0, y=0, z=0; };

class ExploreControl : public rclcpp::Node {
public:
    ExploreControl() : Node("explore_control") {
        declare_parameter("own_ns", "uav3");
        declare_parameter("scout_dist", 20.0);
        declare_parameter("altitude", 20.0);

        own_ns_ = get_parameter("own_ns").as_string();
        double sd = get_parameter("scout_dist").as_double();
        double alt = get_parameter("altitude").as_double();

        wp_ = {
            {0, -sd, alt}, {sd/2, -sd, alt}, {-sd/2, -sd, alt},
            {0, 0, alt}, {sd/2, 0, alt}, {-sd/2, 0, alt}
        };

        pref_ = own_ns_ + "/";
        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            pref_ + "state", 10, [this](const mavros_msgs::msg::State::SharedPtr msg) {
                connected_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
            });

        auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
        pos_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            pref_ + "local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                pos_ = {msg->pose.position.x, msg->pose.position.y, msg->pose.position.z};
            });

        sp_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(pref_ + "setpoint_position/local", 10);
        arming_client_ = create_client<mavros_msgs::srv::CommandBool>(pref_ + "cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>(pref_ + "set_mode");

        sp_timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(100ms, [this]() { update_state(); });

        RCLCPP_INFO(get_logger(), "ExploreControl: %s scout=%.0f alt=%.0f %lu waypoints",
            own_ns_.c_str(), sd, alt, wp_.size());
    }

private:
    void publish_setpoint() {
        if (!connected_) return;
        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";

        const auto& wp = wp_[wp_idx_];
        sp.pose.position.x = wp.x;
        sp.pose.position.y = wp.y;
        sp.pose.position.z = wp.z;
        sp.pose.orientation.w = 1.0;
        sp_pub_->publish(sp);
    }

    void update_state() {
        if (!connected_) return;
        double now_s = now().seconds();
        if (now_s - last_cmd_time_ < 1.0) return;
        last_cmd_time_ = now_s;

        if (mode_ != "OFFBOARD") {
            auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
            req->custom_mode = "OFFBOARD";
            set_mode_client_->async_send_request(req);
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Setting OFFBOARD...");
            return;
        }
        if (!armed_) {
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = true;
            arming_client_->async_send_request(req);
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Arming...");
            return;
        }

        const auto& target = wp_[wp_idx_];
        double dist = std::sqrt(std::pow(pos_.x - target.x, 2) +
                                std::pow(pos_.y - target.y, 2) +
                                std::pow(pos_.z - target.z, 2));
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
            "EXPLORE wp=%lu/%lu dist=%.1f pos=(%.0f,%.0f,%.0f)",
            wp_idx_+1, wp_.size(), dist, pos_.x, pos_.y, pos_.z);

        if (dist < 3.0) {
            wp_idx_ = (wp_idx_ + 1) % wp_.size();
            RCLCPP_INFO(get_logger(), "EXPLORE advancing to waypoint %lu/%lu", wp_idx_+1, wp_.size());
        }
    }

    std::string own_ns_, pref_;
    std::vector<Vec3> wp_;
    size_t wp_idx_ = 0;
    Vec3 pos_;
    bool connected_ = false, armed_ = false;
    std::string mode_;
    double last_cmd_time_ = 0;

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pos_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr sp_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr sp_timer_, cmd_timer_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ExploreControl>());
    rclcpp::shutdown();
    return 0;
}
