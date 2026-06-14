#include "late_fusion_for_yolos_cpp/fusion_config.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace late_fusion_for_yolos_cpp
{
namespace
{

void validateTopics(const std::vector<std::string> & topics, const char * parameter_name)
{
  if (topics.empty()) {
    throw std::invalid_argument(std::string(parameter_name) + " must not be empty");
  }

  std::unordered_set<std::string> unique_topics;
  for (const auto & topic : topics) {
    if (topic.empty()) {
      throw std::invalid_argument(std::string(parameter_name) + " contains an empty topic");
    }
    if (!unique_topics.insert(topic).second) {
      throw std::invalid_argument(
              std::string(parameter_name) + " contains duplicate topic: " + topic);
    }
  }
}

}  // namespace

std::chrono::nanoseconds fusionPeriod(double rate)
{
  if (!std::isfinite(rate) || rate <= 0.0) {
    throw std::invalid_argument("rate must be a finite value greater than zero");
  }

  const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(1.0 / rate));
  if (period.count() <= 0) {
    throw std::invalid_argument("rate is too high to represent as a timer period");
  }
  return period;
}

void validateFusionConfig(const FusionConfig & config)
{
  static_cast<void>(fusionPeriod(config.rate));

  if (!std::isfinite(config.timeout_threshold) || config.timeout_threshold <= 0.0) {
    throw std::invalid_argument("timeout_threshold must be a finite value greater than zero");
  }

  if (config.grid_rows <= 0 || config.grid_cols <= 0) {
    throw std::invalid_argument("grid_rows and grid_cols must be greater than zero");
  }
  if (config.tile_width <= 0 || config.tile_height <= 0) {
    throw std::invalid_argument("tile_width and tile_height must be greater than zero");
  }

  constexpr auto max_int = static_cast<std::int64_t>(std::numeric_limits<int>::max());
  if (config.tile_width > max_int || config.tile_height > max_int) {
    throw std::invalid_argument("tile dimensions exceed OpenCV integer limits");
  }
  if (config.grid_cols > max_int / config.tile_width ||
    config.grid_rows > max_int / config.tile_height)
  {
    throw std::invalid_argument("fused panorama dimensions exceed OpenCV integer limits");
  }
  if (config.grid_rows > std::numeric_limits<std::int64_t>::max() / config.grid_cols) {
    throw std::invalid_argument("grid cell count overflows");
  }

  validateTopics(config.det_topics, "det_inputs");
  validateTopics(config.img_topics, "img_inputs");

  const auto grid_cells = config.grid_rows * config.grid_cols;
  if (static_cast<std::uint64_t>(config.img_topics.size()) >
    static_cast<std::uint64_t>(grid_cells))
  {
    throw std::invalid_argument("grid has fewer cells than img_inputs");
  }

  if (config.output_det_topic.empty() || config.output_img_topic.empty()) {
    throw std::invalid_argument("output_det and output_img must not be empty");
  }
}

}  // namespace late_fusion_for_yolos_cpp
