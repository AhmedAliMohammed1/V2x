// Copyright 2024 YOLOs-CPP Team
// SPDX-License-Identifier: AGPL-3.0

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

#include <opencv2/core.hpp>

#include "ros2_yolos_cpp/conversion/segmentation_converter.hpp"

namespace ros2_yolos_cpp::conversion
{
namespace
{

TEST(SegmentationConverterTest, ConvertsFloatMasksWithoutTypeErrors)
{
  SegmentationResult segmentation;
  segmentation.class_id = 2;
  segmentation.mask = (cv::Mat_<float>(2, 2) << 0.0F, 0.9F, 1.0F, 0.0F);

  const auto message = toCombinedMaskImage({segmentation}, std_msgs::msg::Header(), 2, 2);

  ASSERT_EQ(message.encoding, "mono8");
  ASSERT_EQ(message.data.size(), 4U);
  EXPECT_EQ(message.data[0], 0U);
  EXPECT_EQ(message.data[1], 3U);
  EXPECT_EQ(message.data[2], 3U);
  EXPECT_EQ(message.data[3], 0U);
}

TEST(SegmentationConverterTest, ResizesMasksWithNearestNeighbor)
{
  SegmentationResult segmentation;
  segmentation.class_id = 0;
  segmentation.mask = (cv::Mat_<std::uint8_t>(1, 2) << 0U, 255U);

  const auto message = toCombinedMaskImage({segmentation}, std_msgs::msg::Header(), 4, 1);

  ASSERT_EQ(message.data.size(), 4U);
  EXPECT_EQ(message.data[0], 0U);
  EXPECT_EQ(message.data[1], 0U);
  EXPECT_EQ(message.data[2], 1U);
  EXPECT_EQ(message.data[3], 1U);
}

TEST(SegmentationConverterTest, RejectsClassIdsThatCannotFitMono8)
{
  SegmentationResult segmentation;
  segmentation.class_id = 255;
  segmentation.mask = cv::Mat::ones(1, 1, CV_8UC1);

  EXPECT_THROW(
    toCombinedMaskImage({segmentation}, std_msgs::msg::Header(), 1, 1),
    std::invalid_argument);
}

}  // namespace
}  // namespace ros2_yolos_cpp::conversion
