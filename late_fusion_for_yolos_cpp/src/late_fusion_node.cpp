#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <vision_msgs/msg/detection2_d_array.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "late_fusion_for_yolos_cpp/fusion_config.hpp"

class FusionNode : public rclcpp::Node
{
public:
  FusionNode() : Node("late_fusion_node")
  {
    // ---------------- Parameters ----------------
    declare_parameter("rate", 10.0);
    declare_parameter("det_inputs", std::vector<std::string>());
    declare_parameter("img_inputs", std::vector<std::string>());
    declare_parameter("output_det", "/fused/detections");
    declare_parameter("output_img", "/fused/debug_image");
    declare_parameter("grid_rows", 2);
    declare_parameter("grid_cols", 3);
    declare_parameter("tile_width", 320);
    declare_parameter("tile_height", 240);
    // Timeout in seconds: if no msg for 0.5s, show black screen
    declare_parameter("timeout_threshold", 0.5); 

    late_fusion_for_yolos_cpp::FusionConfig config;
    config.rate = get_parameter("rate").as_double();
    config.det_topics = get_parameter("det_inputs").as_string_array();
    config.img_topics = get_parameter("img_inputs").as_string_array();
    config.output_det_topic = get_parameter("output_det").as_string();
    config.output_img_topic = get_parameter("output_img").as_string();
    config.grid_rows = get_parameter("grid_rows").as_int();
    config.grid_cols = get_parameter("grid_cols").as_int();
    config.tile_width = get_parameter("tile_width").as_int();
    config.tile_height = get_parameter("tile_height").as_int();
    config.timeout_threshold = get_parameter("timeout_threshold").as_double();
    late_fusion_for_yolos_cpp::validateFusionConfig(config);

    rate_ = config.rate;
    det_topics_ = std::move(config.det_topics);
    img_topics_ = std::move(config.img_topics);
    output_det_topic_ = std::move(config.output_det_topic);
    output_img_topic_ = std::move(config.output_img_topic);
    grid_rows_ = static_cast<size_t>(config.grid_rows);
    grid_cols_ = static_cast<size_t>(config.grid_cols);
    tile_width_ = static_cast<int>(config.tile_width);
    tile_height_ = static_cast<int>(config.tile_height);
    timeout_threshold_ = config.timeout_threshold;
    grid_cells_ = grid_rows_ * grid_cols_;

    // ---------------- Subscriptions ----------------
    const auto latest_qos = rclcpp::QoS(rclcpp::KeepLast(1));
    for (const auto & topic : det_topics_) {
      det_subs_.push_back(create_subscription<vision_msgs::msg::Detection2DArray>(
          topic, latest_qos, [this, topic](vision_msgs::msg::Detection2DArray::SharedPtr msg) {
            latest_detections_[topic] = msg;
            last_det_time_[topic] = this->now(); // Record arrival time
          }));
    }

    for (const auto & topic : img_topics_) {
      img_subs_.push_back(create_subscription<sensor_msgs::msg::Image>(
          topic, latest_qos, [this, topic](sensor_msgs::msg::Image::SharedPtr msg) {
            latest_images_[topic] = msg;
            last_img_time_[topic] = this->now(); // Record arrival time
          }));
    }

    // ---------------- Publishers ----------------
    det_pub_ = create_publisher<vision_msgs::msg::Detection2DArray>(output_det_topic_, 10);
    img_pub_ = create_publisher<sensor_msgs::msg::Image>(output_img_topic_, 1);

    // ---------------- Timer ----------------
    timer_ = create_wall_timer(
        late_fusion_for_yolos_cpp::fusionPeriod(rate_),
        std::bind(&FusionNode::publishFusion, this));
  }

private:
  void publishFusion() {
    publishDetections();
    publishImages();
  }

  void publishDetections() {
    vision_msgs::msg::Detection2DArray out_msg;
    out_msg.header.stamp = now();
    out_msg.header.frame_id = "fused";

    for (const auto & topic : det_topics_) {
      if (latest_detections_.count(topic) && latest_detections_[topic]) {
        // Only add detections if the source is still active (fresh)
        double age = (this->now() - last_det_time_[topic]).seconds();
        if (age < timeout_threshold_) {
          for (const auto & det : latest_detections_[topic]->detections)
            out_msg.detections.push_back(det);
        }
      }
    }
    det_pub_->publish(out_msg);
  }

  void publishImages() {
    std::vector<cv::Mat> tiles;
    tiles.reserve(grid_cells_);

    for (size_t i = 0; i < grid_cells_; ++i) {
      bool frame_valid = false;

      if (i < img_topics_.size()) {
        const auto & topic = img_topics_[i];
        
        // CHECK 1: Do we have a message?
        // CHECK 2: Is the message "Fresh" (received recently)?
        if (latest_images_.count(topic) && latest_images_[topic]) {
          double age = (this->now() - last_img_time_[topic]).seconds();
          
          if (age < timeout_threshold_) {
            try {
              const auto cv_image = cv_bridge::toCvShare(latest_images_[topic], "bgr8");
              cv::Mat resized;
              cv::resize(
                cv_image->image, resized, cv::Size(tile_width_, tile_height_), 0.0, 0.0,
                cv::INTER_AREA);
              tiles.push_back(resized);
              frame_valid = true;
            } catch (const std::exception &e) {
              RCLCPP_ERROR(this->get_logger(), "CV Bridge error: %s", e.what());
            }
          }
        }
      }

      // If source is inactive or no topic assigned to this grid cell, show black
      if (!frame_valid) {
        tiles.push_back(cv::Mat::zeros(cv::Size(tile_width_, tile_height_), CV_8UC3));
      }
    }

    // Stitch into Grid
    std::vector<cv::Mat> rows;
    for (size_t r = 0; r < grid_rows_; ++r) {
      std::vector<cv::Mat> row_tiles;
      for (size_t c = 0; c < grid_cols_; ++c) {
        row_tiles.push_back(tiles[r * grid_cols_ + c]);
      }
      cv::Mat combined_row;
      cv::hconcat(row_tiles, combined_row);
      rows.push_back(combined_row);
    }

    cv::Mat panorama;
    cv::vconcat(rows, panorama);

    auto out_img = cv_bridge::CvImage(std_msgs::msg::Header(), "bgr8", panorama).toImageMsg();
    out_img->header.stamp = now();
    out_img->header.frame_id = "fused";
    img_pub_->publish(*out_img);
  }

  // Members
  double rate_, timeout_threshold_;
  size_t grid_rows_, grid_cols_, grid_cells_;
  int tile_width_, tile_height_;
  std::vector<std::string> det_topics_, img_topics_;
  std::string output_det_topic_, output_img_topic_;

  std::vector<rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr> det_subs_;
  std::vector<rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr> img_subs_;
  rclcpp::Publisher<vision_msgs::msg::Detection2DArray>::SharedPtr det_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr img_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::map<std::string, vision_msgs::msg::Detection2DArray::SharedPtr> latest_detections_;
  std::map<std::string, sensor_msgs::msg::Image::SharedPtr> latest_images_;
  
  // Watchdog maps to store the time of last receipt
  std::map<std::string, rclcpp::Time> last_det_time_;
  std::map<std::string, rclcpp::Time> last_img_time_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<FusionNode>());
  } catch (const std::exception & error) {
    RCLCPP_FATAL(rclcpp::get_logger("late_fusion_node"), "Startup failed: %s", error.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
