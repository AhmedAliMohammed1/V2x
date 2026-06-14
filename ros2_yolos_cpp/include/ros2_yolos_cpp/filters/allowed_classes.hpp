// Copyright 2024 YOLOs-CPP Team
// SPDX-License-Identifier: AGPL-3.0

#ifndef ROS2_YOLOS_CPP__FILTERS__ALLOWED_CLASSES_HPP_
#define ROS2_YOLOS_CPP__FILTERS__ALLOWED_CLASSES_HPP_

#include <string>
#include <unordered_set>

#include "ros2_yolos_cpp/visibility_control.hpp"

namespace ros2_yolos_cpp
{

using AllowedClasses = std::unordered_set<std::string>;

/// @brief Load exact class names from a plain-text allowlist.
/// @throws std::runtime_error when the file cannot be read or has no class names.
ROS2_YOLOS_CPP_PUBLIC
AllowedClasses loadAllowedClassesFile(const std::string & path);

}  // namespace ros2_yolos_cpp

#endif  // ROS2_YOLOS_CPP__FILTERS__ALLOWED_CLASSES_HPP_
