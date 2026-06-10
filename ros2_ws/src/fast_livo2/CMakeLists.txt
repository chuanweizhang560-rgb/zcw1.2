cmake_minimum_required(VERSION 3.8)
project(fast_livo2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fexceptions -O3 -march=native -mtune=native -funroll-loops")
add_definitions(-DROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED COMPONENTS common)
find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Sophus REQUIRED)
find_package(vikit_common REQUIRED)
find_package(vikit_ros REQUIRED)
find_library(VIKIT_ROS_LIBRARY vikit_ros PATHS /home/travis/zcw/1.2/ros2_ws/install/vikit_ros/lib NO_DEFAULT_PATH)
find_package(CURL REQUIRED)
find_package(OpenMP QUIET)

# Handle Sophus include directories properly
if(TARGET Sophus::Sophus)
  get_target_property(SOPHUS_INCLUDE_DIR Sophus::Sophus INTERFACE_INCLUDE_DIRECTORIES)
  if(NOT SOPHUS_INCLUDE_DIR)
    set(SOPHUS_INCLUDE_DIR "/opt/ros/humble/include")
  endif()
else()
  set(SOPHUS_INCLUDE_DIR "/opt/ros/humble/include")
endif()

# Get vikit include dirs
if(TARGET vikit_common::vikit_common)
  get_target_property(VIKIT_COMMON_INCLUDE_DIR vikit_common::vikit_common INTERFACE_INCLUDE_DIRECTORIES)
endif()
# vikit_ros doesn't export a target; find include dir from install path
find_path(VIKIT_ROS_INCLUDE_DIR vikit/camera_loader.h
  PATHS /home/travis/zcw/1.2/ros2_ws/install/vikit_ros/include
        /opt/ros/humble/include
  NO_DEFAULT_PATH)

include_directories(
  include
  ${EIGEN3_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${SOPHUS_INCLUDE_DIR}
  ${VIKIT_COMMON_INCLUDE_DIR}
  ${VIKIT_ROS_INCLUDE_DIR}
)

set(SOURCES
  src/LIVMapper.cpp
  src/IMU_Processing.cpp
  src/preprocess.cpp
  src/frame.cpp
  src/vio.cpp
  src/visual_point.cpp
  src/voxel_map.cpp
  src/fast_livo2_node.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ${EIGEN3_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${SOPHUS_INCLUDE_DIR}
  ${VIKIT_COMMON_INCLUDE_DIR}
  ${VIKIT_ROS_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
  Sophus::Sophus
  vikit_common::vikit_common
  ${VIKIT_ROS_LIBRARY}
  ${OpenMP_CXX_LIBRARIES}
)

if(OpenMP_FOUND)
  target_compile_options(${PROJECT_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
endif()

ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  sensor_msgs
  nav_msgs
  geometry_msgs
  visualization_msgs
  cv_bridge
  image_transport
  tf2
  tf2_ros
  tf2_geometry_msgs
  pcl_conversions
)

# Fix curl symbol issue: conda's libcurl lacks CURL_OPENSSL_4
target_link_libraries(${PROJECT_NAME} CURL::libcurl)

install(TARGETS ${PROJECT_NAME} DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
