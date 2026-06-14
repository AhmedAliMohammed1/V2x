#include <gtest/gtest.h>

#include <limits>

#include "late_fusion_for_yolos_cpp/fusion_config.hpp"

namespace late_fusion_for_yolos_cpp
{
namespace
{

FusionConfig validConfig()
{
  FusionConfig config;
  config.det_topics = {"/detector1/detections"};
  config.img_topics = {"/detector1/debug_image"};
  config.grid_rows = 1;
  config.grid_cols = 1;
  return config;
}

TEST(FusionConfigTest, AcceptsValidConfiguration)
{
  EXPECT_NO_THROW(validateFusionConfig(validConfig()));
  EXPECT_GT(fusionPeriod(30.0).count(), 0);
}

TEST(FusionConfigTest, RejectsInvalidRateAndTimeout)
{
  auto config = validConfig();
  config.rate = 0.0;
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);

  config = validConfig();
  config.timeout_threshold = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);
}

TEST(FusionConfigTest, RejectsInvalidGridAndMissingCells)
{
  auto config = validConfig();
  config.grid_rows = 0;
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);

  config = validConfig();
  config.img_topics.push_back("/detector2/debug_image");
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);
}

TEST(FusionConfigTest, RejectsEmptyAndDuplicateTopics)
{
  auto config = validConfig();
  config.det_topics.clear();
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);

  config = validConfig();
  config.img_topics.push_back(config.img_topics.front());
  config.grid_cols = 2;
  EXPECT_THROW(validateFusionConfig(config), std::invalid_argument);
}

}  // namespace
}  // namespace late_fusion_for_yolos_cpp
