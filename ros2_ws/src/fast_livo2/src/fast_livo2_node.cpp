#include "LIVMapper.h"

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto nh = std::make_shared<rclcpp::Node>("laserMapping");
  image_transport::ImageTransport it(nh);
  LIVMapper mapper(nh);
  mapper.initializeSubscribersAndPublishers(nh, it);
  mapper.run();
  rclcpp::shutdown();
  return 0;
}
