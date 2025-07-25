// Copyright (c) 2023 Alberto J. Tudela Roldán
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <string>
#include <limits>
#include <memory>
#include <vector>
#include <utility>

#include "nav2_graceful_controller/parameter_handler.hpp"

namespace nav2_graceful_controller
{

using nav2::declare_parameter_if_not_declared;
using rcl_interfaces::msg::ParameterType;

ParameterHandler::ParameterHandler(
  nav2::LifecycleNode::SharedPtr node, std::string & plugin_name,
  rclcpp::Logger & logger, const double costmap_size_x)
{
  node_ = node;
  plugin_name_ = plugin_name;
  logger_ = logger;

  declare_parameter_if_not_declared(
    node, plugin_name_ + ".transform_tolerance", rclcpp::ParameterValue(0.1));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".min_lookahead", rclcpp::ParameterValue(0.25));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".max_lookahead", rclcpp::ParameterValue(1.0));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".max_robot_pose_search_dist",
    rclcpp::ParameterValue(costmap_size_x / 2.0));
  declare_parameter_if_not_declared(node, plugin_name_ + ".k_phi", rclcpp::ParameterValue(2.0));
  declare_parameter_if_not_declared(node, plugin_name_ + ".k_delta", rclcpp::ParameterValue(1.0));
  declare_parameter_if_not_declared(node, plugin_name_ + ".beta", rclcpp::ParameterValue(0.4));
  declare_parameter_if_not_declared(node, plugin_name_ + ".lambda", rclcpp::ParameterValue(2.0));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".v_linear_min", rclcpp::ParameterValue(0.1));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".v_linear_max", rclcpp::ParameterValue(0.5));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".v_angular_max", rclcpp::ParameterValue(1.0));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".v_angular_min_in_place", rclcpp::ParameterValue(0.25));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".slowdown_radius", rclcpp::ParameterValue(1.5));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".initial_rotation", rclcpp::ParameterValue(true));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".initial_rotation_tolerance", rclcpp::ParameterValue(0.75));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".prefer_final_rotation", rclcpp::ParameterValue(true));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".rotation_scaling_factor", rclcpp::ParameterValue(0.5));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".allow_backward", rclcpp::ParameterValue(false));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".in_place_collision_resolution", rclcpp::ParameterValue(0.1));
  declare_parameter_if_not_declared(
    node, plugin_name_ + ".use_collision_detection", rclcpp::ParameterValue(true));

  node->get_parameter(plugin_name_ + ".transform_tolerance", params_.transform_tolerance);
  node->get_parameter(plugin_name_ + ".min_lookahead", params_.min_lookahead);
  node->get_parameter(plugin_name_ + ".max_lookahead", params_.max_lookahead);
  node->get_parameter(
    plugin_name_ + ".max_robot_pose_search_dist", params_.max_robot_pose_search_dist);
  if (params_.max_robot_pose_search_dist < 0.0) {
    RCLCPP_WARN(
      logger_, "Max robot search distance is negative, setting to max to search"
      " every point on path for the closest value.");
    params_.max_robot_pose_search_dist = std::numeric_limits<double>::max();
  }

  node->get_parameter(plugin_name_ + ".k_phi", params_.k_phi);
  node->get_parameter(plugin_name_ + ".k_delta", params_.k_delta);
  node->get_parameter(plugin_name_ + ".beta", params_.beta);
  node->get_parameter(plugin_name_ + ".lambda", params_.lambda);
  node->get_parameter(plugin_name_ + ".v_linear_min", params_.v_linear_min);
  node->get_parameter(plugin_name_ + ".v_linear_max", params_.v_linear_max);
  params_.v_linear_max_initial = params_.v_linear_max;
  node->get_parameter(plugin_name_ + ".v_angular_max", params_.v_angular_max);
  params_.v_angular_max_initial = params_.v_angular_max;
  node->get_parameter(
    plugin_name_ + ".v_angular_min_in_place", params_.v_angular_min_in_place);
  node->get_parameter(plugin_name_ + ".slowdown_radius", params_.slowdown_radius);
  node->get_parameter(plugin_name_ + ".initial_rotation", params_.initial_rotation);
  node->get_parameter(
    plugin_name_ + ".initial_rotation_tolerance", params_.initial_rotation_tolerance);
  node->get_parameter(plugin_name_ + ".prefer_final_rotation", params_.prefer_final_rotation);
  node->get_parameter(plugin_name_ + ".rotation_scaling_factor", params_.rotation_scaling_factor);
  node->get_parameter(plugin_name_ + ".allow_backward", params_.allow_backward);
  node->get_parameter(
    plugin_name_ + ".in_place_collision_resolution", params_.in_place_collision_resolution);
  node->get_parameter(
    plugin_name_ + ".use_collision_detection", params_.use_collision_detection);

  if (params_.initial_rotation && params_.allow_backward) {
    RCLCPP_WARN(
      logger_, "Initial rotation and allow backward parameters are both true, "
      "setting allow backward to false.");
    params_.allow_backward = false;
  }

  dyn_params_handler_ = node->add_on_set_parameters_callback(
    std::bind(&ParameterHandler::dynamicParametersCallback, this, std::placeholders::_1));
}

ParameterHandler::~ParameterHandler()
{
  auto node = node_.lock();
  if (dyn_params_handler_ && node) {
    node->remove_on_set_parameters_callback(dyn_params_handler_.get());
  }
  dyn_params_handler_.reset();
}

rcl_interfaces::msg::SetParametersResult
ParameterHandler::dynamicParametersCallback(std::vector<rclcpp::Parameter> parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  std::lock_guard<std::mutex> lock_reinit(mutex_);

  for (auto parameter : parameters) {
    const auto & param_type = parameter.get_type();
    const auto & param_name = parameter.get_name();
    if (param_name.find(plugin_name_ + ".") != 0) {
      continue;
    }

    if (param_type == ParameterType::PARAMETER_DOUBLE) {
      if (param_name == plugin_name_ + ".transform_tolerance") {
        params_.transform_tolerance = parameter.as_double();
      } else if (param_name == plugin_name_ + ".min_lookahead") {
        params_.min_lookahead = parameter.as_double();
      } else if (param_name == plugin_name_ + ".max_lookahead") {
        params_.max_lookahead = parameter.as_double();
      } else if (param_name == plugin_name_ + ".k_phi") {
        params_.k_phi = parameter.as_double();
      } else if (param_name == plugin_name_ + ".k_delta") {
        params_.k_delta = parameter.as_double();
      } else if (param_name == plugin_name_ + ".beta") {
        params_.beta = parameter.as_double();
      } else if (param_name == plugin_name_ + ".lambda") {
        params_.lambda = parameter.as_double();
      } else if (param_name == plugin_name_ + ".v_linear_min") {
        params_.v_linear_min = parameter.as_double();
      } else if (param_name == plugin_name_ + ".v_linear_max") {
        params_.v_linear_max = parameter.as_double();
        params_.v_linear_max_initial = params_.v_linear_max;
      } else if (param_name == plugin_name_ + ".v_angular_max") {
        params_.v_angular_max = parameter.as_double();
        params_.v_angular_max_initial = params_.v_angular_max;
      } else if (param_name == plugin_name_ + ".v_angular_min_in_place") {
        params_.v_angular_min_in_place = parameter.as_double();
      } else if (param_name == plugin_name_ + ".slowdown_radius") {
        params_.slowdown_radius = parameter.as_double();
      } else if (param_name == plugin_name_ + ".initial_rotation_tolerance") {
        params_.initial_rotation_tolerance = parameter.as_double();
      } else if (param_name == plugin_name_ + ".rotation_scaling_factor") {
        params_.rotation_scaling_factor = parameter.as_double();
      } else if (param_name == plugin_name_ + ".in_place_collision_resolution") {
        params_.in_place_collision_resolution = parameter.as_double();
      }
    } else if (param_type == ParameterType::PARAMETER_BOOL) {
      if (param_name == plugin_name_ + ".initial_rotation") {
        if (parameter.as_bool() && params_.allow_backward) {
          RCLCPP_WARN(
            logger_, "Initial rotation and allow backward parameters are both true, "
            "rejecting parameter change.");
          continue;
        }
        params_.initial_rotation = parameter.as_bool();
      } else if (param_name == plugin_name_ + ".prefer_final_rotation") {
        params_.prefer_final_rotation = parameter.as_bool();
      } else if (param_name == plugin_name_ + ".allow_backward") {
        if (params_.initial_rotation && parameter.as_bool()) {
          RCLCPP_WARN(
            logger_, "Initial rotation and allow backward parameters are both true, "
            "rejecting parameter change.");
          continue;
        }
        params_.allow_backward = parameter.as_bool();
      } else if (param_name == plugin_name_ + ".use_collision_detection") {
        params_.use_collision_detection = parameter.as_bool();
      }
    }
  }

  result.successful = true;
  return result;
}

}  // namespace nav2_graceful_controller
