#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>
#include <vector>

using namespace std::chrono_literals;

enum class State { APPROACH, DESCEND, TRACK, RECAPTURE, FALLBACK, DONE };

struct Vec3 {
    double x = 0, y = 0, z = 0;
    double dist(const Vec3& o) const {
        return std::sqrt((x-o.x)*(x-o.x)+(y-o.y)*(y-o.y)+(z-o.z)*(z-o.z));
    }
};

class CableTracker : public rclcpp::Node {
public:
    CableTracker() : Node("cable_tracker"), connected_(false), armed_(false) {
        declare_parameter("tower_x1", -30.0);
        declare_parameter("tower_x2", 30.0);
        declare_parameter("cable_y", 0.6);
        declare_parameter("attach_height", 25.0);
        declare_parameter("sag", 4.0);
        declare_parameter("track_speed", 3.0);
        declare_parameter("recapture_dist", 5.0);
        declare_parameter("recapture_timeout", 5.0);

        x1_ = get_parameter("tower_x1").as_double();
        x2_ = get_parameter("tower_x2").as_double();
        cy_ = get_parameter("cable_y").as_double();
        ah_ = get_parameter("attach_height").as_double();
        sag_ = get_parameter("sag").as_double();
        speed_ = get_parameter("track_speed").as_double();
        recapture_dist_ = get_parameter("recapture_dist").as_double();
        recapture_timeout_ = get_parameter("recapture_timeout").as_double();

        span_half_ = (x2_ - x1_) / 2.0;
        cx_ = (x1_ + x2_) / 2.0;
        cable_len_ = std::abs(x2_ - x1_);

        state_sub_ = create_subscription<mavros_msgs::msg::State>(
            "mavros/state", 10,
            [this](const mavros_msgs::msg::State::SharedPtr msg) {
                connected_ = msg->connected;
                armed_ = msg->armed;
                mode_ = msg->mode;
            });

        auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
        pos_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
            "mavros/local_position/pose", qos,
            [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
                pos_ = {msg->pose.position.x, msg->pose.position.y, msg->pose.position.z};
            });

        sp_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
            "mavros/setpoint_position/local", 10);

        arming_client_ = create_client<mavros_msgs::srv::CommandBool>("mavros/cmd/arming");
        set_mode_client_ = create_client<mavros_msgs::srv::SetMode>("mavros/set_mode");

        sp_timer_ = create_wall_timer(100ms, [this]() { publish_setpoint(); });
        cmd_timer_ = create_wall_timer(100ms, [this]() { update_state(); });

        state_ = State::APPROACH;
        t_progress_ = 0.0;
        error_start_ = 0.0;
        recapture_count_ = 0;
        last_good_t_ = 0.0;

        RCLCPP_INFO(get_logger(), "CableTracker started: x=[%.0f,%.0f] y=%.1f h=%.1f sag=%.1f",
            x1_, x2_, cy_, ah_, sag_);
    }

private:
    Vec3 cable_point(double t) const {
        double x = x1_ + t * (x2_ - x1_);
        double xr = (x - cx_) / span_half_;
        return {x, cy_, ah_ - sag_ * (1 - xr * xr)};
    }

    Vec3 cable_tangent(double t) const {
        double x = x1_ + t * (x2_ - x1_);
        double xr = (x - cx_) / span_half_;
        double dx = x2_ - x1_;
        double dz = -2 * sag_ * xr * (dx / span_half_);
        double len = std::sqrt(dx*dx + dz*dz);
        return {dx/len, 0, dz/len};
    }

    double find_closest_t(const Vec3& p) const {
        double best_t = 0, best_d = 1e9;
        for (int i = 0; i <= 50; i++) {
            double t = i / 50.0;
            double d = p.dist(cable_point(t));
            if (d < best_d) { best_d = d; best_t = t; }
        }
        return best_t;
    }

    void publish_setpoint() {
        if (state_ == State::DONE) return;
        geometry_msgs::msg::PoseStamped sp;
        sp.header.stamp = now();
        sp.header.frame_id = "map";

        switch (state_) {
        case State::APPROACH: {
            auto wp = cable_point(t_progress_);
            sp.pose.position.x = wp.x; sp.pose.position.y = wp.y; sp.pose.position.z = ah_ + 5.0;
            break;
        }
        case State::DESCEND: {
            auto wp = cable_point(t_progress_);
            sp.pose.position.x = wp.x; sp.pose.position.y = wp.y; sp.pose.position.z = wp.z;
            break;
        }
        case State::TRACK: {
            auto sp_t = cable_point(t_progress_);
            sp.pose.position.x = sp_t.x; sp.pose.position.y = sp_t.y; sp.pose.position.z = sp_t.z;
            break;
        }
        case State::RECAPTURE: {
            auto wp = cable_point(last_good_t_);
            sp.pose.position.x = wp.x; sp.pose.position.y = wp.y + 3.0; sp.pose.position.z = wp.z + 2.0;
            break;
        }
        case State::FALLBACK: {
            sp.pose.position.x = 0; sp.pose.position.y = 0; sp.pose.position.z = 15.0;
            break;
        }
        case State::DONE: {
            sp.pose.position.x = 0; sp.pose.position.y = 0; sp.pose.position.z = 15.0;
            break;
        }
        default: break;
        }

        auto tan = cable_tangent(t_progress_);
        double yaw = std::atan2(tan.y, tan.x);
        sp.pose.orientation.x = 0; sp.pose.orientation.y = 0;
        sp.pose.orientation.z = std::sin(yaw/2); sp.pose.orientation.w = std::cos(yaw/2);
        sp_pub_->publish(sp);
    }

    void update_state() {
        if (!connected_) return;
        if (state_ == State::DONE) return;

        // Initialization: ARM and set OFFBOARD before entering state machine
        if (!armed_) {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "Arming...");
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = true;
            arming_client_->async_send_request(req);
            return;
        }
        if (mode_ != "OFFBOARD") {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "Setting OFFBOARD...");
            auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
            req->custom_mode = "OFFBOARD";
            set_mode_client_->async_send_request(req);
            return;
        }

        double error = pos_.dist(cable_point(find_closest_t(pos_)));

        switch (state_) {
        case State::APPROACH: {
            auto wp = cable_point(t_progress_);
            if (pos_.dist({wp.x, wp.y, ah_+5.0}) < 2.0) {
                state_ = State::DESCEND;
                RCLCPP_INFO(get_logger(), "DESCEND to cable");
            }
            break;
        }
        case State::DESCEND: {
            auto wp = cable_point(t_progress_);
            if (pos_.dist(wp) < 2.0) {
                state_ = State::TRACK;
                last_good_t_ = t_progress_;
                RCLCPP_INFO(get_logger(), "TRACK starting");
            }
            break;
        }
        case State::TRACK: {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
                "TRACK t=%.2f pos=(%.1f,%.1f,%.1f) err=%.2f",
                t_progress_, pos_.x, pos_.y, pos_.z, error);
            // Monotonically advance along cable; never go backward
            double closest = find_closest_t(pos_);
            if (error < recapture_dist_) {
                error_start_ = 0;
                last_good_t_ = closest;
                t_progress_ = std::min(t_progress_ + 0.003 * speed_, 1.0);
            } else {
                if (error_start_ == 0) error_start_ = now().seconds();
                else if ((now().seconds() - error_start_) > recapture_timeout_) {
                    state_ = State::RECAPTURE;
                    recapture_count_++;
                    error_start_ = 0;
                    RCLCPP_WARN(get_logger(), "LOST (err=%.1f)! RECAPTURE #%d", error, recapture_count_);
                }
            }

            if (t_progress_ >= 1.0) {
                state_ = State::DONE;
                RCLCPP_INFO(get_logger(), "TRACK complete! Arrived at tower1. Disarming...");
                auto disarm_req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                disarm_req->value = false;
                arming_client_->async_send_request(disarm_req);
            }
            break;
        }
        case State::RECAPTURE: {
            auto wp = cable_point(last_good_t_);
            if (pos_.dist({wp.x, wp.y+3.0, wp.z+2.0}) < 3.0) {
                double search_t = std::max(0.0, last_good_t_ - 0.1);
                t_progress_ = search_t;
                state_ = State::TRACK;
                RCLCPP_INFO(get_logger(), "Re-acquired, resuming TRACK");
            } else if (recapture_count_ > 3) {
                state_ = State::FALLBACK;
                RCLCPP_WARN(get_logger(), "RECAPTURE failed, FALLBACK to home");
            }
            break;
        }
        case State::FALLBACK: {
            if (pos_.dist({0,0,15.0}) < 2.0) {
                RCLCPP_INFO(get_logger(), "Home, ready to land");
                state_ = State::DONE;
            }
            break;
        }
        default: break;
        }
    }

    rclcpp::Subscription<mavros_msgs::msg::State>::SharedPtr state_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pos_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr sp_pub_;
    rclcpp::Client<mavros_msgs::srv::CommandBool>::SharedPtr arming_client_;
    rclcpp::Client<mavros_msgs::srv::SetMode>::SharedPtr set_mode_client_;
    rclcpp::TimerBase::SharedPtr sp_timer_, cmd_timer_;

    double x1_, x2_, cy_, ah_, sag_, speed_;
    double recapture_dist_, recapture_timeout_;
    double span_half_, cx_, cable_len_;

    State state_;
    double t_progress_, error_start_;
    int recapture_count_;
    double last_good_t_;
    Vec3 pos_;
    bool connected_, armed_;
    std::string mode_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CableTracker>());
    rclcpp::shutdown();
    return 0;
}
