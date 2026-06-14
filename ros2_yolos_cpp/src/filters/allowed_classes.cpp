// Copyright 2024 YOLOs-CPP Team
// SPDX-License-Identifier: AGPL-3.0

#include "ros2_yolos_cpp/filters/allowed_classes.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace ros2_yolos_cpp
{
namespace
{

std::string trim(const std::string & value)
{
  constexpr char whitespace[] = " \t\r\n";
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string::npos) {
    return {};
  }

  const auto last = value.find_last_not_of(whitespace);
  return value.substr(first, last - first + 1);
}

}  // namespace

AllowedClasses loadAllowedClassesFile(const std::string & path)
{
  if (path.empty()) {
    throw std::runtime_error("allowed_classes_path is empty");
  }

  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Failed to open allowed classes file: " + path);
  }

  AllowedClasses allowed_classes;
  std::string line;
  while (std::getline(input, line)) {
    auto class_name = trim(line);
    if (class_name.empty() || class_name.front() == '#') {
      continue;
    }
    allowed_classes.insert(std::move(class_name));
  }

  if (input.bad()) {
    throw std::runtime_error("Failed while reading allowed classes file: " + path);
  }
  if (allowed_classes.empty()) {
    throw std::runtime_error("Allowed classes file contains no class names: " + path);
  }

  return allowed_classes;
}

}  // namespace ros2_yolos_cpp
