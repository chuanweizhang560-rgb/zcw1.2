#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace std::chrono_literals;

struct Vec3 { double x=0, y=0, z=0; };

// Cable catenary math (matches cable_env.py)
static Vec3 cable_point(double t, double x1=-30.0, double x2=30.0,
                         double cy=0.6, double ah=25.0, double sag=4.0) {
    double x = x1 + t * (x2 - x1);
    double cx = (x1 + x2) / 2.0;
    double span = (x2 - x1) / 2.0;
    double xr = (x - cx) / span;
    double z = ah - sag * (1 - xr * xr);
    return {x, cy, z};
}

class CableTrackerRL : public rclcpp::Node {
public:
    CableTrackerRL() : Node("cable_tracker_rl") {
        declare_parameter("tower_x1", -30.0);
        declare_parameter("tower_x2", 30.0);
        declare_parameter("cable_y", 0.6);
        declare_parameter("attach_height", 25.0);
        declare_parameter("sag", 4.0);
        declare_parameter("mavros_ns", "");
        declare_parameter("model_path", "");

        x1_ = get_parameter("tower_x1").as_double();
        x2_ = get_parameter("tower_x2").as_double();
        cy_ = get_parameter("cable_y").as_double();
        ah_ = get_parameter("attach_height").as_double();
        sag_ = get_parameter("sag").as_double();
        std::string model_path = get_parameter("model_path").as_string();

        span_half_ = (x2_ - x1_) / 2.0;
        cx_ = (x1_ + x2_) / 2.0;

        std::string pref = get_parameter("mavros_ns").as_string();
        if (!pref.empty()) pref += "/";

        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            pref + "state", 10, [this](const mavros_msgs::msg::State::SharedPtr msg) {
                connected_ = msg->connected; armed_ = msg->armed; mode_ = msg->mode;
            });

        auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
        pos_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            pref + "local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                pos_ = {msg->pose.position.x, msg->pose.position.y, msg->pose.position.z};
            });

        sp_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(pref + "setpoint_position/local", 10);
        arming_client_ = create_client<mavros_msgs::srv::CommandBool>(pref + "cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>(pref + "set_mode");

        sp_timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(100ms, [this]() { update_state(); });

        // Load PPO model via Python subprocess
        model_loaded_ = false;
        if (!model_path.empty()) {
            load_model(model_path);
        }

        t_progress_ = 0.0;
        RCLCPP_INFO(get_logger(), "CableTrackerRL: x=[%.0f,%.0f] model=%s",
            x1_, x2_, model_path.c_str());
    }

private:
    void load_model(const std::string& path) {
        // Try loading model via Python subprocess
        std::string cmd = "python3 -c \""
            "from stable_baselines3 import PPO;"
            "m = PPO.load('" + path + "');"
            "print('model_ok:' + str(m.observation_space.shape[0]));"
            "\" 2>/dev/null";
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) { RCLCPP_WARN(get_logger(), "Cannot run Python"); return; }
        char buf[128];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string s(buf);
            if (s.find("model_ok:") == 0) {
                model_loaded_ = true;
                model_path_ = path;
                RCLCPP_INFO(get_logger(), "PPO model loaded: %s", path.c_str());
            }
        }
        pclose(fp);
    }

    void publish_setpoint() {
        if (!connected_) return;
        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";
        sp.pose.orientation.w = 1.0;

        auto wp = cable_point(t_progress_, x1_, x2_, cy_, ah_, sag_);
        double action_dt = 0.0, action_dy = 0.0, action_dz = 0.0;

        if (model_loaded_) {
            // Run inference via Python
            Vec3 err = {pos_.x - wp.x, pos_.y - wp.y, pos_.z - wp.z};
            std::string py_cmd = "python3 -c \""
                "import numpy as np;"
                "from stable_baselines3 import PPO;"
                "m = PPO.load('" + model_path_ + "');"
                "obs = np.array([[" + std::to_string(t_progress_) + ","
                + std::to_string(err.x) + "," + std::to_string(err.y) + ","
                + std::to_string(err.z) + "]]);"
                "a = m.predict(obs, deterministic=True)[0][0];"
                "print(f'{a[0]:.6f},{a[1]:.6f},{a[2]:.6f}');"
                "\" 2>/dev/null";
            FILE* fp = popen(py_cmd.c_str(), "r");
            if (fp) {
                char buf[128];
                if (fgets(buf, sizeof(buf), fp)) {
                    std::string s(buf);
                    // Trim whitespace
                    s.erase(s.find_last_not_of(" \n\r\t") + 1);
                    if (sscanf(s.c_str(), "%lf,%lf,%lf", &action_dt, &action_dy, &action_dz) == 3) {
                        action_dt = std::clamp(action_dt, -1.0, 1.0) * 0.03;
                        t_progress_ = std::clamp(t_progress_ + action_dt + 0.015, 0.0, 1.0);
                        wp = cable_point(t_progress_, x1_, x2_, cy_, ah_, sag_);
                        sp.pose.position.x = wp.x + action_dy * 2.0;
                        sp.pose.position.y = wp.y + action_dy * 2.0;
                        sp.pose.position.z = wp.z + action_dz * 2.0;
                    }
                }
                pclose(fp);
            }
        } else {
            // Fallback: follow cable at constant speed
            t_progress_ = std::min(t_progress_ + 0.003 * 3.0, 1.0);
            wp = cable_point(t_progress_, x1_, x2_, cy_, ah_, sag_);
            sp.pose.position.x = wp.x;
            sp.pose.position.y = wp.y;
            sp.pose.position.z = wp.z + 1.0;
        }

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
            return;
        }
        if (!armed_) {
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = true;
            arming_client_->async_send_request(req);
            return;
        }

        auto wp = cable_point(t_progress_, x1_, x2_, cy_, ah_, sag_);
        double err = std::sqrt(std::pow(pos_.x - wp.x, 2) +
                               std::pow(pos_.y - wp.y, 2) +
                               std::pow(pos_.z - wp.z, 2));

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
            "RL t=%.2f err=%.2f pos=(%.1f,%.1f,%.1f)%s",
            t_progress_, err, pos_.x, pos_.y, pos_.z,
            model_loaded_ ? " [RL]" : " [fallback]");

        if (t_progress_ >= 1.0) {
            RCLCPP_INFO(get_logger(), "RL tracker DONE");
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = false;
            arming_client_->async_send_request(req);
        }
    }

    double x1_, x2_, cy_, ah_, sag_, span_half_, cx_;
    double t_progress_ = 0, last_cmd_time_ = 0;
    Vec3 pos_;
    bool connected_ = false, armed_ = false, model_loaded_ = false;
    std::string mode_;
    std::string model_path_;

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pos_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr sp_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr sp_timer_, cmd_timer_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CableTrackerRL>());
    rclcpp::shutdown();
    return 0;
}
