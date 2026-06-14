cmake_minimum_required(VERSION 3.8)
project(late_fusion_for_yolos_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(OpenCV REQUIRED)

add_library(late_fusion_config src/fusion_config.cpp)
target_include_directories(late_fusion_config PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_executable(late_fusion_node src/late_fusion_node.cpp)

ament_target_dependencies(late_fusion_node
  rclcpp
  sensor_msgs
  vision_msgs
  cv_bridge
  image_transport
)

target_link_libraries(late_fusion_node late_fusion_config ${OpenCV_LIBRARIES})

install(TARGETS late_fusion_config late_fusion_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_fusion_config test/test_fusion_config.cpp)
  target_link_libraries(test_fusion_config late_fusion_config)
endif()

ament_package()
