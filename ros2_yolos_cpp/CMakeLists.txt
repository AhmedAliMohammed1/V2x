# =============================================================================
# ros2_yolos_cpp - ROS 2 Wrapper for YOLOs-CPP Inference Library
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(ros2_yolos_cpp VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# =============================================================================
# Build Options
# =============================================================================
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_GPU "Enable CUDA GPU support" ON)

# =============================================================================
# Find ROS 2 Dependencies
# =============================================================================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# =============================================================================
# Find OpenCV
# =============================================================================
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# =============================================================================
# ONNX Runtime
# =============================================================================

set(ONNXRUNTIME_DIR "" CACHE PATH "Path to ONNX Runtime installation")

if(NOT ONNXRUNTIME_DIR AND DEFINED ENV{ONNXRUNTIME_DIR})
  set(ONNXRUNTIME_DIR $ENV{ONNXRUNTIME_DIR})
endif()

if(NOT ONNXRUNTIME_DIR)
  set(ONNXRUNTIME_DIR "/ros2_ws/onnxruntime")
endif()

message(STATUS "ONNXRUNTIME_DIR: ${ONNXRUNTIME_DIR}")

find_library(ONNXRUNTIME_LIB
  NAMES onnxruntime libonnxruntime.so
  PATHS
    ${ONNXRUNTIME_DIR}/lib
    /ros2_ws/onnxruntime/lib
    /opt/onnxruntime/lib
    /usr/local/lib
    $ENV{ONNXRUNTIME_DIR}/lib
)

find_path(ONNXRUNTIME_INCLUDE_DIR
  NAMES onnxruntime_cxx_api.h
  PATHS
    ${ONNXRUNTIME_DIR}/include
    ${ONNXRUNTIME_DIR}/include/onnxruntime
    ${ONNXRUNTIME_DIR}/include/onnxruntime/core/session
    /ros2_ws/onnxruntime/include
    /ros2_ws/onnxruntime/include/onnxruntime
    /ros2_ws/onnxruntime/include/onnxruntime/core/session
    /opt/onnxruntime/include
    /opt/onnxruntime/include/onnxruntime
    /opt/onnxruntime/include/onnxruntime/core/session
    /usr/local/include
    /usr/local/include/onnxruntime
    /usr/local/include/onnxruntime/core/session
    $ENV{ONNXRUNTIME_DIR}/include
    $ENV{ONNXRUNTIME_DIR}/include/onnxruntime
    $ENV{ONNXRUNTIME_DIR}/include/onnxruntime/core/session
)

if(NOT ONNXRUNTIME_LIB)
  message(FATAL_ERROR "ONNX Runtime library not found. Set -DONNXRUNTIME_DIR=/path/to/onnxruntime")
endif()

if(NOT ONNXRUNTIME_INCLUDE_DIR)
  message(FATAL_ERROR "ONNX Runtime headers not found. Set -DONNXRUNTIME_DIR=/path/to/onnxruntime")
endif()

message(STATUS "ONNX Runtime include: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "ONNX Runtime lib: ${ONNXRUNTIME_LIB}")

# =============================================================================
# YOLOs-CPP Library - Download at Configuration Time
# =============================================================================
set(YOLOS_CPP_VERSION "master")
set(YOLOS_CPP_DIR "${CMAKE_BINARY_DIR}/YOLOs-CPP")
set(YOLOS_CPP_URL "https://github.com/Geekgineer/YOLOs-CPP/archive/refs/heads/${YOLOS_CPP_VERSION}.tar.gz")

if(NOT EXISTS "${YOLOS_CPP_DIR}/include/yolos/yolos.hpp")
  message(STATUS "Downloading YOLOs-CPP from: ${YOLOS_CPP_URL}")

  file(DOWNLOAD
    "${YOLOS_CPP_URL}"
    "${CMAKE_BINARY_DIR}/yolos_cpp.tar.gz"
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS
  )

  list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
  if(NOT STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download YOLOs-CPP: ${DOWNLOAD_STATUS}")
  endif()

  message(STATUS "Extracting YOLOs-CPP...")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_BINARY_DIR}/yolos_cpp.tar.gz"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    RESULT_VARIABLE EXTRACT_RESULT
  )

  if(NOT EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to extract YOLOs-CPP")
  endif()

  file(GLOB EXTRACTED_DIRS "${CMAKE_BINARY_DIR}/YOLOs-CPP-*")
  if(NOT EXTRACTED_DIRS)
    message(FATAL_ERROR "Extraction failed: Could not find YOLOs-CPP-* directory in ${CMAKE_BINARY_DIR}")
  endif()

  list(GET EXTRACTED_DIRS 0 EXTRACTED_DIR)

  file(REMOVE_RECURSE "${YOLOS_CPP_DIR}")
  file(RENAME "${EXTRACTED_DIR}" "${YOLOS_CPP_DIR}")
  file(REMOVE "${CMAKE_BINARY_DIR}/yolos_cpp.tar.gz")
endif()

message(STATUS "YOLOs-CPP Source: ${YOLOS_CPP_DIR}")

# =============================================================================
# Custom Messages
# =============================================================================
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/OrientedBoundingBox2D.msg"
  "msg/OBBDetection2D.msg"
  "msg/OBBDetection2DArray.msg"
  "msg/KeyPoint2D.msg"
  "msg/KeyPoint2DArray.msg"
  DEPENDENCIES std_msgs geometry_msgs
)

# =============================================================================
# Include Directories
# =============================================================================
include_directories(
  include
  ${YOLOS_CPP_DIR}/include
  ${ONNXRUNTIME_INCLUDE_DIR}
)

# =============================================================================
# Component Library
# =============================================================================
add_library(${PROJECT_NAME}_components SHARED
  src/adapters/detector_adapter.cpp
  src/adapters/segmentor_adapter.cpp
  src/adapters/pose_adapter.cpp
  src/adapters/obb_adapter.cpp
  src/adapters/classifier_adapter.cpp
  src/conversion/detection_converter.cpp
  src/conversion/segmentation_converter.cpp
  src/conversion/pose_converter.cpp
  src/conversion/obb_converter.cpp
  src/conversion/classification_converter.cpp
  src/nodes/detector_node.cpp
  src/nodes/segmentor_node.cpp
  src/nodes/pose_node.cpp
  src/nodes/obb_node.cpp
  src/nodes/classifier_node.cpp
)

target_include_directories(${PROJECT_NAME}_components PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}_components
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)

ament_target_dependencies(${PROJECT_NAME}_components
  rclcpp
  rclcpp_lifecycle
  rclcpp_components
  image_transport
  cv_bridge
  sensor_msgs
  vision_msgs
  std_msgs
  geometry_msgs
  lifecycle_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(${PROJECT_NAME}_components "${cpp_typesupport_target}")

# =============================================================================
# Register Components
# =============================================================================
rclcpp_components_register_node(${PROJECT_NAME}_components
  PLUGIN "ros2_yolos_cpp::YolosDetectorNode"
  EXECUTABLE yolos_detector_node
)

rclcpp_components_register_node(${PROJECT_NAME}_components
  PLUGIN "ros2_yolos_cpp::YolosSegmentorNode"
  EXECUTABLE yolos_segmentor_node
)

rclcpp_components_register_node(${PROJECT_NAME}_components
  PLUGIN "ros2_yolos_cpp::YolosPoseNode"
  EXECUTABLE yolos_pose_node
)

rclcpp_components_register_node(${PROJECT_NAME}_components
  PLUGIN "ros2_yolos_cpp::YolosOBBNode"
  EXECUTABLE yolos_obb_node
)

rclcpp_components_register_node(${PROJECT_NAME}_components
  PLUGIN "ros2_yolos_cpp::YolosClassifierNode"
  EXECUTABLE yolos_classifier_node
)

# =============================================================================
# RPATH for ONNX Runtime
# =============================================================================
if(UNIX AND NOT APPLE)
  get_filename_component(ONNXRUNTIME_LIB_DIR ${ONNXRUNTIME_LIB} DIRECTORY)
  set_target_properties(${PROJECT_NAME}_components PROPERTIES
    BUILD_RPATH "${ONNXRUNTIME_LIB_DIR}"
    INSTALL_RPATH "${ONNXRUNTIME_LIB_DIR}"
  )
endif()

# =============================================================================
# Installation
# =============================================================================
install(TARGETS ${PROJECT_NAME}_components
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

# =============================================================================
# Testing
# =============================================================================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_detection_converter
    test/unit/test_detection_converter.cpp
  )
  target_link_libraries(test_detection_converter ${PROJECT_NAME}_components)
  ament_target_dependencies(test_detection_converter rclcpp vision_msgs)

  macro(add_integration_test target_name source_file)
    ament_add_gtest(${target_name} ${source_file})
    target_link_libraries(${target_name}
      ${PROJECT_NAME}_components
      ${OpenCV_LIBS}
    )
    ament_target_dependencies(${target_name} rclcpp rclcpp_lifecycle)
  endmacro()

  add_integration_test(test_integration_detection test/integration/test_detection.cpp)
  add_integration_test(test_integration_segmentation test/integration/test_segmentation.cpp)
  add_integration_test(test_integration_pose test/integration/test_pose.cpp)
  add_integration_test(test_integration_obb test/integration/test_obb.cpp)
  add_integration_test(test_integration_classification test/integration/test_classification.cpp)
endif()

# =============================================================================
# Export
# =============================================================================
ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_components)
ament_export_dependencies(
  rclcpp
  rclcpp_lifecycle
  rclcpp_components
  image_transport
  cv_bridge
  sensor_msgs
  vision_msgs
)

ament_package()

message(STATUS "")
message(STATUS "=== ros2_yolos_cpp Configuration ===")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV:        ${OpenCV_VERSION}")
message(STATUS "  ONNX Runtime:  ${ONNXRUNTIME_LIB}")
message(STATUS "  ONNX Include:  ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "  YOLOs-CPP:     ${YOLOS_CPP_DIR}")
message(STATUS "  GPU Support:   ${ENABLE_GPU}")
message(STATUS "")
