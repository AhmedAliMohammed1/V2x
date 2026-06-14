// Copyright 2024 YOLOs-CPP Team
// SPDX-License-Identifier: AGPL-3.0

#include "ros2_yolos_cpp/nodes/detector_node.hpp"
#include "ros2_yolos_cpp/conversion/detection_converter.hpp"
#include <ament_index_cpp/get_package_share_directory.hpp>
#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif
#include <chrono>
#include <cstdlib>
#include <lifecycle_msgs/msg/state.hpp>

namespace ros2_yolos_cpp {

YolosDetectorNode::YolosDetectorNode(const rclcpp::NodeOptions& options)
  : rclcpp_lifecycle::LifecycleNode("yolos_detector", options) {
  RCLCPP_INFO(get_logger(), "YolosDetectorNode created");
  declareParameters();
  autostart_ = get_parameter("autostart").as_bool();
  if (autostart_) {
    autostart_timer_ = create_wall_timer(
      std::chrono::milliseconds(250),
      std::bind(&YolosDetectorNode::autostart, this));
  }
}

void YolosDetectorNode::declareParameters() {
  const auto default_allowed_classes_path =
    ament_index_cpp::get_package_share_directory("ros2_yolos_cpp") +
    "/config/allowed_classes.txt";
  declare_parameter("model_path", rclcpp::PARAMETER_STRING);
  declare_parameter("labels_path", rclcpp::PARAMETER_STRING);
  declare_parameter("allowed_classes_path", default_allowed_classes_path);
  declare_parameter("autostart", false);
  declare_parameter("use_gpu", false);
  declare_parameter("conf_threshold", 0.4);
  declare_parameter("nms_threshold", 0.45);
  declare_parameter("yolo_version", "auto");
  declare_parameter("publish_debug_image", false);
  declare_parameter("publish_timing", false);
}

[[noreturn]] void YolosDetectorNode::exitAfterFatalInferenceError(const char* message) {
  RCLCPP_FATAL(
    get_logger(),
    "Fatal GPU inference error: %s. Exiting so launch can recreate the CUDA context.",
    message);
  std::_Exit(EXIT_FAILURE);
}

void YolosDetectorNode::autostart() {
  autostart_timer_->cancel();

  auto state = configure();
  if (state.id() != lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
    RCLCPP_ERROR(get_logger(), "Autostart configuration failed; node remains unconfigured");
    return;
  }

  state = activate();
  if (state.id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
    RCLCPP_ERROR(get_logger(), "Autostart activation failed; node remains inactive");
  }
}

YolosConfig YolosDetectorNode::loadConfig() {
  YolosConfig config;
  config.model_path = get_parameter("model_path").as_string();
  config.labels_path = get_parameter("labels_path").as_string();
  config.allowed_classes_path = get_parameter("allowed_classes_path").as_string();
  config.use_gpu = get_parameter("use_gpu").as_bool();
  config.conf_threshold = static_cast<float>(get_parameter("conf_threshold").as_double());
  config.nms_threshold = static_cast<float>(get_parameter("nms_threshold").as_double());
  config.yolo_version = get_parameter("yolo_version").as_string();
  
  model_path_ = config.model_path;
  labels_path_ = config.labels_path;
  allowed_classes_path_ = config.allowed_classes_path;
  use_gpu_ = config.use_gpu;
  conf_threshold_ = config.conf_threshold;
  nms_threshold_ = config.nms_threshold;
  yolo_version_ = config.yolo_version;
  publish_debug_image_ = get_parameter("publish_debug_image").as_bool();
  publish_timing_ = get_parameter("publish_timing").as_bool();

  return config;
}

YolosDetectorNode::CallbackReturn YolosDetectorNode::on_configure(const rclcpp_lifecycle::State&) {
  RCLCPP_INFO(get_logger(), "Configuring...");
  try {
    auto config = loadConfig();
    if (config.model_path.empty() || config.labels_path.empty()) {
      RCLCPP_ERROR(get_logger(), "model_path and labels_path are required");
      return CallbackReturn::FAILURE;
    }
    if (!isValidProbability(config.conf_threshold) ||
      !isValidProbability(config.nms_threshold))
    {
      RCLCPP_ERROR(get_logger(), "conf_threshold and nms_threshold must be between 0 and 1");
      return CallbackReturn::FAILURE;
    }
    detector_ = createDetectorAdapter();
    if (!detector_->initialize(config)) {
      RCLCPP_ERROR(get_logger(), "Failed to initialize detector");
      return CallbackReturn::FAILURE;
    }
    inference_cb_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    det_pub_ = create_publisher<vision_msgs::msg::Detection2DArray>("~/detections", 10);
    if (publish_debug_image_) debug_pub_ = create_publisher<sensor_msgs::msg::Image>("~/debug_image", 1);
    if (publish_timing_) timing_pub_ = create_publisher<std_msgs::msg::Float64MultiArray>("~/timing", 10);
    RCLCPP_INFO(get_logger(), "Configured successfully");
    return CallbackReturn::SUCCESS;
  } catch (const FatalInferenceError& e) {
    exitAfterFatalInferenceError(e.what());
  } catch (const std::exception& e) {
    RCLCPP_ERROR(get_logger(), "Configuration failed: %s", e.what());
    return CallbackReturn::FAILURE;
  }
}

YolosDetectorNode::CallbackReturn YolosDetectorNode::on_activate(const rclcpp_lifecycle::State&) {
  RCLCPP_INFO(get_logger(), "Activating...");
  det_pub_->on_activate();
  if (debug_pub_) debug_pub_->on_activate();
  if (timing_pub_) timing_pub_->on_activate();

  auto sub_options = rclcpp::SubscriptionOptions();
  sub_options.callback_group = inference_cb_group_;
  auto image_qos = rclcpp::SensorDataQoS().keep_last(1);
  image_sub_ = create_subscription<sensor_msgs::msg::Image>(
    "~/image_raw", image_qos,
    std::bind(&YolosDetectorNode::imageCallback, this, std::placeholders::_1),
    sub_options);

  RCLCPP_INFO(get_logger(), "Activated - listening on ~/image_raw");
  return CallbackReturn::SUCCESS;
}

YolosDetectorNode::CallbackReturn YolosDetectorNode::on_deactivate(const rclcpp_lifecycle::State&) {
  image_sub_.reset();
  det_pub_->on_deactivate();
  if (debug_pub_) debug_pub_->on_deactivate();
  if (timing_pub_) timing_pub_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

YolosDetectorNode::CallbackReturn YolosDetectorNode::on_cleanup(const rclcpp_lifecycle::State&) {
  if (detector_) { detector_->shutdown(); detector_.reset(); }
  det_pub_.reset(); debug_pub_.reset(); timing_pub_.reset();
  return CallbackReturn::SUCCESS;
}

YolosDetectorNode::CallbackReturn YolosDetectorNode::on_shutdown(const rclcpp_lifecycle::State& s) {
  return on_cleanup(s);
}

void YolosDetectorNode::imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr& msg) {
  if (!detector_ || !detector_->isInitialized()) return;
  auto t_start = std::chrono::high_resolution_clock::now();
  try {
    auto cv_ptr = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::BGR8);
    auto t_preprocess = std::chrono::high_resolution_clock::now();
    auto detections = detector_->detect(cv_ptr->image, conf_threshold_, nms_threshold_);
    auto t_inference = std::chrono::high_resolution_clock::now();
    det_pub_->publish(conversion::toDetection2DArray(detections, msg->header, msg->width, msg->height));
    auto t_postprocess = std::chrono::high_resolution_clock::now();

    if (publish_debug_image_ && debug_pub_ && debug_pub_->is_activated()) {
      cv::Mat debug = cv_ptr->image.clone();
      detector_->drawDetections(debug, detections);
      debug_pub_->publish(*cv_bridge::CvImage(msg->header, "bgr8", debug).toImageMsg());
    }
    if (publish_timing_ && timing_pub_ && timing_pub_->is_activated()) {
      std_msgs::msg::Float64MultiArray timing;
      timing.data = {
        std::chrono::duration<double, std::milli>(t_preprocess - t_start).count(),
        std::chrono::duration<double, std::milli>(t_inference - t_preprocess).count(),
        std::chrono::duration<double, std::milli>(t_postprocess - t_inference).count()
      };
      timing_pub_->publish(timing);
    }
  } catch (const FatalInferenceError& e) {
    exitAfterFatalInferenceError(e.what());
  } catch (const std::exception& e) {
    if (isFatalGpuErrorMessage(e.what())) {
      exitAfterFatalInferenceError(e.what());
    }
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Detection failed: %s", e.what());
  }
}

}  // namespace ros2_yolos_cpp

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(ros2_yolos_cpp::YolosDetectorNode)
