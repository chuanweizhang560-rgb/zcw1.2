#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

class InspectionControl : public rclcpp::Node {
public:
    InspectionControl() : Node("inspection_control"), state_(false), armed_(false) {
        declare_parameter("radius", 80.0);
        declare_parameter("height", 60.0);
        declare_parameter("angular_velocity", 0.15);
        declare_parameter("center_x", 80.0);
        declare_parameter("center_y", 0.0);

        radius_ = get_parameter("radius").as_double();
        height_ = get_parameter("height").as_double();
        omega_ = get_parameter("angular_velocity").as_double();
        cx_ = get_parameter("center_x").as_double();
        cy_ = get_parameter("center_y").as_double();

        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            "mavros/state", 10,
            [this](const mavros_msgs::msg::State::SharedPtr msg) {
                state_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
            });

        local_pos_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
            "mavros/setpoint_position/local", 10);

        arming_client_ = create_client<mavros_msgs::srv::CommandBool>("mavros/cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("mavros/set_mode");

        timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(1s, [this]() { send_commands(); });

        RCLCPP_INFO(get_logger(), "InspectionControl initialized: radius=%.1fm height=%.1fm omega=%.2frad/s",
            radius_, height_, omega_);
    }

private:
    void publish_setpoint() {
        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";

        double t = (now().seconds() - start_time_);
        double angle = omega_ * t;

        sp.pose.position.x = cx_ + radius_ * std::cos(angle);
        sp.pose.position.y = cy_ + radius_ * std::sin(angle);
        sp.pose.position.z = height_;

        double yaw = std::atan2(cy_ - sp.pose.position.y, cx_ - sp.pose.position.x);
        sp.pose.orientation.x = 0;
        sp.pose.orientation.y = 0;
        sp.pose.orientation.z = std::sin(yaw / 2);
        sp.pose.orientation.w = std::cos(yaw / 2);

        local_pos_pub_->publish(sp);
    }

    void send_commands() {
        static int step = 0;
        if (!state_) {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Waiting for MAVROS...");
            return;
        }

        switch (step) {
        case 0:
            if (!armed_) {
                RCLCPP_INFO(get_logger(), "Arming...");
                auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                req->value = true;
                arming_client_->async_send_request(req);
            } else {
                RCLCPP_INFO(get_logger(), "Armed, setting OFFBOARD...");
                step = 1;
            }
            break;
        case 1:
            if (mode_ != "OFFBOARD") {
                RCLCPP_INFO(get_logger(), "Setting OFFBOARD mode...");
                auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
                req->custom_mode = "OFFBOARD";
                set_mode_client_->async_send_request(req);
            } else {
                RCLCPP_INFO(get_logger(), "OFFBOARD active, starting orbit...");
                start_time_ = now().seconds();
                step = 2;
            }
            break;
        case 2:
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
                "Orbiting: R=%.1f H=%.1f ω=%.2f", radius_, height_, omega_);
            break;
        }
    }

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr local_pos_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::TimerBase::SharedPtr cmd_timer_;

    double radius_;
    double height_;
    double omega_;
    double cx_;
    double cy_;
    double start_time_;
    bool state_;
    bool armed_;
    std::string mode_;
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<InspectionControl>());
    rclcpp::shutdown();
    return 0;
}
