#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <mavros_msgs/msg/state.hpp>
#include <mavros_msgs/srv/command_bool.hpp>
#include <mavros_msgs/srv/set_mode.hpp>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

enum class State { APPROACH, DESCEND, TRACK, SWEEP, FALLBACK, LAND, DONE };

struct Vec3 {
    double x = 0, y = 0, z = 0;
    double dist(const Vec3& o) const {
        return std::sqrt((x-o.x)*(x-o.x)+(y-o.y)*(y-o.y)+(z-o.z)*(z-o.z));
    }
};

class CableTracker : public rclcpp::Node {
public:
    CableTracker() : Node("cable_tracker") {
        declare_parameter("tower_x1", -30.0);
        declare_parameter("tower_x2", 30.0);
        declare_parameter("cable_y", 0.6);
        declare_parameter("attach_height", 25.0);
        declare_parameter("sag", 4.0);
        declare_parameter("track_speed", 3.0);
        declare_parameter("recapture_dist", 5.0);
        declare_parameter("sweep_timeout", 8.0);
        std::string mavros_ns = declare_parameter("mavros_ns", "");

        x1_ = get_parameter("tower_x1").as_double();
        x2_ = get_parameter("tower_x2").as_double();
        cy_ = get_parameter("cable_y").as_double();
        ah_ = get_parameter("attach_height").as_double();
        sag_ = get_parameter("sag").as_double();
        speed_ = get_parameter("track_speed").as_double();
        recapture_dist_ = get_parameter("recapture_dist").as_double();
        sweep_timeout_ = get_parameter("sweep_timeout").as_double();

        span_half_ = (x2_ - x1_) / 2.0;
        cx_ = (x1_ + x2_) / 2.0;

        pref_ = mavros_ns.empty() ? "" : mavros_ns + "/";
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

        state_ = State::APPROACH;
        t_progress_ = 0.0;
        error_start_ = 0.0;
        recapture_count_ = 0;
        last_good_t_ = 0.0;
        sweep_amp_ = 2.0;
        land_target_z_ = 15.0;

        RCLCPP_INFO(get_logger(), "CableTracker v2 start: x=[%.0f,%.0f] y=%.1f h=%.1f sag=%.1f",
            x1_, x2_, cy_, ah_, sag_);
    }

private:
    Vec3 cable_point(double t) const {
        double x = x1_ + t * (x2_ - x1_);
        double xr = (x - cx_) / span_half_;
        return {x, cy_, ah_ - sag_ * (1 - xr * xr)};
    }

    Vec3 cable_normal(double t) const {
        double x = x1_ + t * (x2_ - x1_);
        double xr = (x - cx_) / span_half_;
        double dx = x2_ - x1_;
        double dz = -2 * sag_ * xr * (dx / span_half_);
        double len = std::sqrt(dx*dx + dz*dz);
        return {dz/len, 0, -dx/len};
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
        case State::SWEEP: {
            auto wp = cable_point(last_good_t_);
            double osc = std::sin(sweep_phase_) * sweep_amp_;
            sp.pose.position.x = wp.x;
            sp.pose.position.y = wp.y + osc;
            sp.pose.position.z = wp.z + 3.0;
            sweep_phase_ += 0.02;
            break;
        }
        case State::FALLBACK:
        case State::LAND: {
            sp.pose.position.x = 0; sp.pose.position.y = 0; sp.pose.position.z = land_target_z_;
            break;
        }
        default: break;
        }

        double yaw = t_progress_ >= 1.0 ? 0 : std::atan2(x2_-x1_, 0);
        sp.pose.orientation.x = 0; sp.pose.orientation.y = 0;
        sp.pose.orientation.z = std::sin(yaw/2); sp.pose.orientation.w = std::cos(yaw/2);
        sp_pub_->publish(sp);
    }

    void log_state(const char* to, const char* reason) {
        RCLCPP_INFO(get_logger(), "%s: %s | t=%.2f err=%.1f pos=(%.1f,%.1f,%.1f)",
            to, reason, t_progress_, pos_.dist(cable_point(find_closest_t(pos_))),
            pos_.x, pos_.y, pos_.z);
    }

    void update_state() {
        if (!connected_ || state_ == State::DONE) return;

        auto now_s = now().seconds();
        double cmd_cooldown = 1.0;

        if (mode_ != "OFFBOARD") {
            if (state_ == State::FALLBACK || state_ == State::LAND || state_ == State::DONE) return;
            if (now_s - last_cmd_time_ < cmd_cooldown) return;
            last_cmd_time_ = now_s;
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "Setting OFFBOARD...");
            auto req = std::make_shared<mavros_msgs::srv::SetMode::Request>();
            req->custom_mode = "OFFBOARD";
            set_mode_client_->async_send_request(req);
            return;
        }
        if (!armed_) {
            if (now_s - last_cmd_time_ < cmd_cooldown) return;
            last_cmd_time_ = now_s;
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "Arming...");
            auto req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
            req->value = true;
            arming_client_->async_send_request(req);
            return;
        }

        double closest_t = find_closest_t(pos_);
        double error = pos_.dist(cable_point(closest_t));

        switch (state_) {
        case State::APPROACH: {
            auto wp = cable_point(t_progress_);
            if (pos_.dist({wp.x, wp.y, ah_+5.0}) < 2.0) {
                state_ = State::DESCEND;
                log_state("DESCEND", "arrived at cable start");
            }
            break;
        }
        case State::DESCEND: {
            auto wp = cable_point(t_progress_);
            if (pos_.dist(wp) < 2.0) {
                state_ = State::TRACK;
                last_good_t_ = t_progress_;
                log_state("TRACK", "descended to cable");
            }
            break;
        }
        case State::TRACK: {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
                "TRACK t=%.2f pos=(%.1f,%.1f,%.1f) err=%.2f",
                t_progress_, pos_.x, pos_.y, pos_.z, error);

            if (error < recapture_dist_) {
                error_start_ = 0;
                last_good_t_ = closest_t;
                t_progress_ = std::min(t_progress_ + 0.003 * speed_, 1.0);
            } else {
                double elapsed = (error_start_ == 0) ? 0 : (now().seconds() - error_start_);
                if (error_start_ == 0) error_start_ = now().seconds();
                else if (elapsed >= sweep_timeout_) {
                    state_ = State::SWEEP;
                    recapture_count_++;
                    error_start_ = 0;
                    sweep_phase_ = 0;
                    sweep_amp_ = 2.0 + recapture_count_ * 2.0;
                    log_state("SWEEP", "cable lost, searching");
                }
            }

            if (t_progress_ >= 1.0) {
                log_state("DONE", "reached tower1");
                auto disarm_req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                disarm_req->value = false;
                arming_client_->async_send_request(disarm_req);
                state_ = State::DONE;
            }
            break;
        }
        case State::SWEEP: {
            double elapsed = now().seconds() - error_start_;
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000,
                "SWEEP #%d amp=%.0f err=%.1f elapsed=%.0fs",
                recapture_count_, sweep_amp_, error, elapsed);

            if (error < recapture_dist_ * 0.7) {
                state_ = State::TRACK;
                t_progress_ = std::max(0.0, last_good_t_ - 0.05);
                log_state("TRACK", "reacquired during sweep");
            } else if (elapsed > sweep_timeout_ || recapture_count_ > 3) {
                state_ = State::FALLBACK;
                log_state("FALLBACK", "sweep exhausted, returning home");
                land_target_z_ = 15.0;
            }
            break;
        }
        case State::FALLBACK: {
            RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000,
                "FALLBACK pos=(%.0f,%.0f,%.1f) home_z=%.1f",
                pos_.x, pos_.y, pos_.z, land_target_z_);

            if (pos_.dist({0,0,land_target_z_}) < 3.0) {
                if (land_target_z_ > 2.0) {
                    land_target_z_ = std::max(2.0, land_target_z_ - 5.0);
                } else {
                    state_ = State::LAND;
                    log_state("LAND", "descending to ground");
                }
            }
            break;
        }
        case State::LAND: {
            if (pos_.z < 1.0) {
                log_state("DONE", "landed");
                auto disarm_req = std::make_shared<mavros_msgs::srv::CommandBool::Request>();
                disarm_req->value = false;
                arming_client_->async_send_request(disarm_req);
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
    double recapture_dist_, sweep_timeout_;
    double span_half_, cx_;

    State state_;
    double t_progress_, error_start_;
    int recapture_count_;
    double last_good_t_;
    std::string pref_;
    double sweep_phase_, sweep_amp_;
    double last_cmd_time_ = 0;
    double land_target_z_;
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
