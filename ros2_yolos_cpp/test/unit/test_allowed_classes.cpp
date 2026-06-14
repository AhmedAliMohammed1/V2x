// Copyright 2024 YOLOs-CPP Team
// SPDX-License-Identifier: AGPL-3.0

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "ros2_yolos_cpp/filters/allowed_classes.hpp"

namespace ros2_yolos_cpp::test
{

class AllowedClassesTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    path_ = std::filesystem::temp_directory_path() /
      "ros2_yolos_cpp_allowed_classes_test.txt";
    std::filesystem::remove(path_);
  }

  void TearDown() override
  {
    std::filesystem::remove(path_);
  }

  void writeFile(const std::string & contents)
  {
    std::ofstream output(path_);
    ASSERT_TRUE(output.is_open());
    output << contents;
    ASSERT_TRUE(output.good());
  }

  std::filesystem::path path_;
};

TEST_F(AllowedClassesTest, LoadsClassesAndIgnoresWhitespaceCommentsAndDuplicates) {
  writeFile(
    "# Detection classes\n"
    "\n"
    " car \n"
    "truck\n"
    "traffic light\r\n"
    "person\n"
    "car\n");

  const auto allowed_classes = loadAllowedClassesFile(path_.string());

  EXPECT_EQ(allowed_classes.size(), 4u);
  EXPECT_EQ(allowed_classes.count("car"), 1u);
  EXPECT_EQ(allowed_classes.count("truck"), 1u);
  EXPECT_EQ(allowed_classes.count("traffic light"), 1u);
  EXPECT_EQ(allowed_classes.count("person"), 1u);
  EXPECT_EQ(allowed_classes.count(" car "), 0u);
}

TEST_F(AllowedClassesTest, RejectsMissingFile) {
  EXPECT_THROW(loadAllowedClassesFile(path_.string()), std::runtime_error);
}

TEST_F(AllowedClassesTest, RejectsEmptyPath) {
  EXPECT_THROW(loadAllowedClassesFile(""), std::runtime_error);
}

TEST_F(AllowedClassesTest, RejectsFileWithoutClasses) {
  writeFile("# comments only\n  \n\t\n");

  EXPECT_THROW(loadAllowedClassesFile(path_.string()), std::runtime_error);
}

}  // namespace ros2_yolos_cpp::test
