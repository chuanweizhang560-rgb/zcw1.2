#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

struct Vec3 {
    double x = 0, y = 0, z = 0;
    double dist(const Vec3& o) const {
        return std::sqrt((x-o.x)*(x-o.x)+(y-o.y)*(y-o.y)+(z-o.z)*(z-o.z));
    }
};

class RelayControl : public rclcpp::Node {
public:
    RelayControl() : Node("relay_control") {
        declare_parameter("relay_distance", 30.0);
        declare_parameter("min_z", 20.0);
        relay_dist_ = get_parameter("relay_distance").as_double();
        min_z_ = get_parameter("min_z").as_double();

        std::string own_ns = declare_parameter("own_ns", "");
        std::string inspect_ns = declare_parameter("inspect_ns", "");

        own_pref_ = own_ns.empty() ? "" : own_ns + "/";
        inspect_pref_ = inspect_ns.empty() ? "" : inspect_ns + "/";

        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            own_pref_ + "state", 10,
            [this](const mavros_msgs::msg::State::SharedPtr msg) {
                connected_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
            });

        auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
        pos_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            own_pref_ + "local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                pos_ = {msg->pose.position.x, msg->pose.position.y, msg->pose.position.z};
            });

        inspect_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            inspect_pref_ + "local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                inspect_pos_ = {msg->pose.position.x, msg->pose.position.y, msg->pose.position.z};
                inspect_valid_ = true;
            });

        sp_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
            own_pref_ + "setpoint_position/local", 10);
        arming_client_ = create_client<mavros_msgs::srv::CommandBool>(own_pref_ + "cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>(own_pref_ + "set_mode");

        sp_timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(100ms, [this]() { update_state(); });

        RCLCPP_INFO(get_logger(), "RelayControl: relay=%.0fm own=%s inspect=%s",
            relay_dist_, own_ns.c_str(), inspect_ns.c_str());
    }

private:
    Vec3 compute_relay_pos() const {
        Vec3 d = {inspect_pos_.x, inspect_pos_.y, 0};
        double len = std::sqrt(d.x*d.x + d.y*d.y);
        if (len < 1.0) return {0, 0, min_z_};
        double ux = d.x / len, uy = d.y / len;
        double behind = std::min(relay_dist_, len * 0.4);
        return {inspect_pos_.x - ux * behind,
                inspect_pos_.y - uy * behind,
                std::max(min_z_, inspect_pos_.z)};
    }

    void publish_setpoint() {
        if (!connected_) return;
        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";
        Vec3 t = compute_relay_pos();
        sp.pose.position.x = t.x;
        sp.pose.position.y = t.y;
        sp.pose.position.z = t.z;
        sp.pose.orientation.w = 1.0;
        sp_pub_->publish(sp);
    }
    void update_state() {
        if (!connected_) return;
        auto now_s = now().seconds();

        if (mode_ != "OFFBOARD") {
            if (now_s - last_cmd_time_ < 1.0) return;
            last_cmd_time_ = now_s;
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Setting OFFBOARD...");
            auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
            req->custom_mode = "OFFBOARD";
            set_mode_client_->async_send_request(req);
            return;
        }
        if (!armed_) {
            if (now_s - last_cmd_time_ < 1.0) return;
            last_cmd_time_ = now_s;
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000, "Arming relay...");
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = true;
            arming_client_->async_send_request(req);
            return;
        }
        if (!inspect_valid_) {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for inspect pos...");
            return;
        }
        Vec3 t = compute_relay_pos();
        double err = pos_.dist(t);
        double d = pos_.dist(inspect_pos_);
        if (d > relay_dist_ * 3.0)
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 10000,
                "TOPOLOGY: inspect-relay=%.0fm", d);
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
            "RELAY: inspect(%.0f,%.0f) relay_set(%.0f,%.0f) err=%.1f dist=%.0f",
            inspect_pos_.x, inspect_pos_.y, t.x, t.y, err, d);
    }

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pos_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr inspect_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr sp_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr sp_timer_, cmd_timer_;
    double relay_dist_, min_z_;
    Vec3 pos_, inspect_pos_;
    bool connected_ = false, armed_ = false, inspect_valid_ = false;
    std::string mode_, own_pref_, inspect_pref_;
    double last_cmd_time_ = 0;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RelayControl>());
    rclcpp::shutdown();
    return 0;
}
