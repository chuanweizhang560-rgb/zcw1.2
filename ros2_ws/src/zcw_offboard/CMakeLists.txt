cmake_minimum_required(VERSION 3.8)
project(zcw_offboard)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mavros_msgs REQUIRED)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_executable(offboard_control src/offboard_control.cpp)
ament_target_dependencies(offboard_control rclcpp geometry_msgs mavros_msgs)

add_executable(inspection_control src/inspection_control.cpp)
ament_target_dependencies(inspection_control rclcpp geometry_msgs mavros_msgs)

add_executable(cable_follow_control src/cable_follow_control.cpp)
ament_target_dependencies(cable_follow_control rclcpp geometry_msgs mavros_msgs)

add_executable(cable_tracker src/cable_tracker.cpp)
ament_target_dependencies(cable_tracker rclcpp geometry_msgs mavros_msgs)

install(TARGETS
  offboard_control
  inspection_control
  cable_follow_control
  cable_tracker
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
