#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <chrono>
#include <array>
#include <sstream>

using namespace std::chrono_literals;

enum class Role { INSPECT, RELAY, EXPLORE, RESERVE, UNASSIGNED };

struct UAVState {
    std::string ns;
    bool connected = false;
    bool armed = false;
    std::string mode;
    double x = 0, y = 0, z = 0;
    Role role = Role::UNASSIGNED;
};

class RoleAllocator : public rclcpp::Node {
public:
    RoleAllocator() : Node("role_allocator") {
        declare_parameter("num_uavs", 4);
        int n = get_parameter("num_uavs").as_int();
        num_uavs_ = n;

        states_.resize(n);
        for (int i = 0; i < n; i++) {
            std::string ns = "uav" + std::to_string(i + 1);
            states_[i].ns = ns;

            state_subs_.push_back(create_subscription<mavros_msgs::msg::State>(
                ns + "/state", 10,
                [this, i](const mavros_msgs::msg::State::SharedPtr msg) {
                    states_[i].connected = msg->connected;
                    states_[i].armed = msg->armed;
                    states_[i].mode = msg->mode;
                }));

            auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
            pos_subs_.push_back(create_subscription<geometry_msgs::msg::PoseStamped>(
                ns + "/local_position/pose", qos,
                [this, i](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                    states_[i].x = msg->pose.position.x;
                    states_[i].y = msg->pose.position.y;
                    states_[i].z = msg->pose.position.z;
                }));
        }

        role_pubs_.resize(n);
        for (int i = 0; i < n; i++) {
            role_pubs_[i] = create_publisher<std_msgs::msg::String>(
                states_[i].ns + "/role", 10);
        }

        alloc_timer_ = create_wall_timer(1000ms, [this]() { allocate(); });
        report_timer_ = create_wall_timer(5000ms, [this]() { report(); });

        RCLCPP_INFO(get_logger(), "RoleAllocator: %d UAVs", n);
    }

private:
    void allocate() {
        // V0 static allocation based on UAV index
        std::array<Role, 4> roles = {
            Role::INSPECT,  // uav1: inspect cable
            Role::RELAY,    // uav2: relay for uav1
            Role::EXPLORE,  // uav3: scout / recapture assist
            Role::RESERVE   // uav4: standby
        };

        for (size_t i = 0; i < states_.size() && i < roles.size(); i++) {
            if (states_[i].role != roles[i]) {
                states_[i].role = roles[i];
                std_msgs::msg::String msg;
                switch (roles[i]) {
                    case Role::INSPECT: msg.data = "INSPECT"; break;
                    case Role::RELAY:   msg.data = "RELAY";   break;
                    case Role::EXPLORE: msg.data = "EXPLORE";  break;
                    case Role::RESERVE: msg.data = "RESERVE";  break;
                    default:            msg.data = "UNASSIGNED";
                }
                role_pubs_[i]->publish(msg);
                RCLCPP_INFO(get_logger(), "UAV%d -> %s", (int)i+1, msg.data.c_str());
            }
        }
    }

    void report() {
        std::stringstream ss;
        ss << "--- RoleAllocator Report ---\n";
        for (size_t i = 0; i < states_.size(); i++) {
            const auto& s = states_[i];
            std::string role_str;
            switch (s.role) {
                case Role::INSPECT:  role_str = "INSPECT";  break;
                case Role::RELAY:    role_str = "RELAY";    break;
                case Role::EXPLORE:  role_str = "EXPLORE";  break;
                case Role::RESERVE:  role_str = "RESERVE";  break;
                default:             role_str = "NONE";      break;
            }
            ss << "  " << s.ns << ": " << role_str
               << " | " << (s.connected ? "CONN" : "OFF")
               << " " << s.mode
               << " " << (s.armed ? "ARM" : "DIS")
               << " | pos=(" << std::round(s.x) << "," << std::round(s.y) << "," << std::round(s.z) << ")\n";
        }
        RCLCPP_INFO(get_logger(), "%s", ss.str().c_str());
    }

    int num_uavs_;
    std::vector<UAVState> states_;
    std::vector<rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr> state_subs_;
    std::vector<rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr> pos_subs_;
    std::vector<rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> role_pubs_;
    rclcpp::TimerBase::SharedPtr alloc_timer_, report_timer_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RoleAllocator>());
    rclcpp::shutdown();
    return 0;
}
