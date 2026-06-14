#ifndef LATE_FUSION_FOR_YOLOS_CPP__FUSION_CONFIG_HPP_
#define LATE_FUSION_FOR_YOLOS_CPP__FUSION_CONFIG_HPP_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace late_fusion_for_yolos_cpp
{

struct FusionConfig
{
  double rate{10.0};
  double timeout_threshold{0.5};
  std::int64_t grid_rows{2};
  std::int64_t grid_cols{3};
  std::int64_t tile_width{320};
  std::int64_t tile_height{240};
  std::vector<std::string> det_topics;
  std::vector<std::string> img_topics;
  std::string output_det_topic{"/fused/detections"};
  std::string output_img_topic{"/fused/debug_image"};
};

void validateFusionConfig(const FusionConfig & config);

std::chrono::nanoseconds fusionPeriod(double rate);

}  // namespace late_fusion_for_yolos_cpp

#endif  // LATE_FUSION_FOR_YOLOS_CPP__FUSION_CONFIG_HPP_
