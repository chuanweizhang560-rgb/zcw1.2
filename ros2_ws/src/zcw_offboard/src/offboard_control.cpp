#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <mavros_msgs/srv/command_tol.hpp>
#include <chrono>

using namespace std::chrono_literals;

class OffboardControl : public rclcpp::Node {
public:
    OffboardControl() : Node("offboard_control"), state_(false), armed_(false) {
        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            "mavros/state", 10,
            [this](const mavros_msgs::msg::State::SharedPtr msg) {
                state_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
                RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000,
                    "State: connected=%d armed=%d mode=%s",
                    msg->connected, msg->armed, msg->mode.c_str());
            });

        local_pos_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
            "mavros/setpoint_position/local", 10);

        arming_client_ = create_client<mavros_msgs::srv::CommandBool>("mavros/cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("mavros/set_mode");

        timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(1s, [this]() { send_commands(); });

        setpoint_.pose.position.x = 0;
        setpoint_.pose.position.y = 0;
        setpoint_.pose.position.z = 10;
    }

private:
    void publish_setpoint() {
        setpoint_.header.stamp = now();
        local_pos_pub_->publish(setpoint_);
    }

    void send_commands() {
        static int step = 0;
        if (!state_) {
            RCLCPP_INFO(get_logger(), "Waiting for MAVROS connection...");
            return;
        }

        switch (step) {
        case 0:
            if (!armed_) {
                RCLCPP_INFO(get_logger(), "Arming...");
                auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                req->value = true;
                arming_client_->async_send_request(req,
                    [this](rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedFuture) {
                        RCLCPP_INFO(get_logger(), "Arm command sent");
                    });
            } else {
                RCLCPP_INFO(get_logger(), "Already armed, setting OFFBOARD mode...");
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
                RCLCPP_INFO(get_logger(), "OFFBOARD mode active, flying to 10m...");
                step = 2;
            }
            break;

        case 2:
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000,
                "Holding position at 10m...");
            break;
        }
    }

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr local_pos_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::TimerBase::SharedPtr cmd_timer_;

    geometry_msgs::msg::PoseStamped setpoint_;
    bool state_;
    bool armed_;
    std::string mode_;
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OffboardControl>());
    rclcpp::shutdown();
    return 0;
}
