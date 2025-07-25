// Copyright (c) 2018 Intel Corporation
// Copyright (c) 2021 Samsung Research America
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

#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "nav_msgs/msg/path.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_util/geometry_utils.hpp"

#include "behaviortree_cpp/bt_factory.h"

#include "nav2_behavior_tree/utils/test_action_server.hpp"
#include "nav2_behavior_tree/plugins/action/remove_passed_goals_action.hpp"
#include "utils/test_behavior_tree_fixture.hpp"

class RemovePassedGoalsTestFixture : public ::testing::Test
{
public:
  static void SetUpTestCase()
  {
    node_ = std::make_shared<nav2::LifecycleNode>("passed_goals_test_fixture");
    factory_ = std::make_shared<BT::BehaviorTreeFactory>();

    config_ = new BT::NodeConfiguration();
    transform_handler_ = std::make_shared<nav2_behavior_tree::TransformHandler>(node_);
    transform_handler_->activate();

    // Create the blackboard that will be shared by all of the nodes in the tree
    config_->blackboard = BT::Blackboard::create();
    // Put items on the blackboard
    config_->blackboard->set(
      "node",
      node_);
    config_->blackboard->set(
      "tf_buffer",
      transform_handler_->getBuffer());

    BT::NodeBuilder builder =
      [](const std::string & name, const BT::NodeConfiguration & config)
      {
        return std::make_unique<nav2_behavior_tree::RemovePassedGoals>(
          name, config);
      };

    factory_->registerBuilder<nav2_behavior_tree::RemovePassedGoals>(
      "RemovePassedGoals", builder);
  }

  static void TearDownTestCase()
  {
    transform_handler_->deactivate();
    delete config_;
    config_ = nullptr;
    transform_handler_.reset();
    node_.reset();
    factory_.reset();
  }

  void TearDown() override
  {
    tree_.reset();
  }

protected:
  static nav2::LifecycleNode::SharedPtr node_;
  static BT::NodeConfiguration * config_;
  static std::shared_ptr<BT::BehaviorTreeFactory> factory_;
  static std::shared_ptr<BT::Tree> tree_;
  static std::shared_ptr<nav2_behavior_tree::TransformHandler> transform_handler_;
};

nav2::LifecycleNode::SharedPtr RemovePassedGoalsTestFixture::node_ = nullptr;

BT::NodeConfiguration * RemovePassedGoalsTestFixture::config_ = nullptr;
std::shared_ptr<BT::BehaviorTreeFactory> RemovePassedGoalsTestFixture::factory_ = nullptr;
std::shared_ptr<BT::Tree> RemovePassedGoalsTestFixture::tree_ = nullptr;
std::shared_ptr<nav2_behavior_tree::TransformHandler>
RemovePassedGoalsTestFixture::transform_handler_ = nullptr;

TEST_F(RemovePassedGoalsTestFixture, test_tick)
{
  geometry_msgs::msg::Pose pose;
  pose.position.x = 0.25;
  pose.position.y = 0.0;

  transform_handler_->waitForTransform();
  transform_handler_->updateRobotPose(pose);

  // create tree
  std::string xml_txt =
    R"(
      <root BTCPP_format="4">
        <BehaviorTree ID="MainTree">
          <RemovePassedGoals radius="0.5" input_goals="{goals}" output_goals="{goals}"/>
        </BehaviorTree>
      </root>)";

  tree_ = std::make_shared<BT::Tree>(factory_->createTreeFromText(xml_txt, config_->blackboard));

  // create new goal and set it on blackboard
  nav_msgs::msg::Goals poses;
  poses.goals.resize(4);
  poses.goals[0].pose.position.x = 0.0;
  poses.goals[0].pose.position.y = 0.0;

  poses.goals[1].pose.position.x = 0.5;
  poses.goals[1].pose.position.y = 0.0;

  poses.goals[2].pose.position.x = 1.0;
  poses.goals[2].pose.position.y = 0.0;

  poses.goals[3].pose.position.x = 2.0;
  poses.goals[3].pose.position.y = 0.0;

  config_->blackboard->set("goals", poses);

  // tick until node succeeds
  while (tree_->rootNode()->status() != BT::NodeStatus::SUCCESS) {
    tree_->rootNode()->executeTick();
  }

  // check that it removed the point in range
  nav_msgs::msg::Goals output_poses;
  EXPECT_TRUE(config_->blackboard->get("goals", output_poses));

  EXPECT_EQ(output_poses.goals.size(), 2u);
  EXPECT_EQ(output_poses.goals[0], poses.goals[2]);
  EXPECT_EQ(output_poses.goals[1], poses.goals[3]);
}

TEST_F(RemovePassedGoalsTestFixture,
  test_tick_remove_passed_goals_success_and_output_waypoint_statuses)
{
  geometry_msgs::msg::Pose pose;
  pose.position.x = 0.25;
  pose.position.y = 0.0;

  transform_handler_->waitForTransform();
  transform_handler_->updateRobotPose(pose);

  // create tree
  std::string xml_txt =
    R"(
      <root BTCPP_format="4">
        <BehaviorTree ID="MainTree">
          <RemovePassedGoals radius="0.5" input_goals="{goals}" output_goals="{goals}"
                             input_waypoint_statuses="{waypoint_statuses}"
                             output_waypoint_statuses="{waypoint_statuses}"/>
        </BehaviorTree>
      </root>)";

  tree_ = std::make_shared<BT::Tree>(factory_->createTreeFromText(xml_txt, config_->blackboard));

  // create new goal and set it on blackboard
  nav_msgs::msg::Goals poses;
  poses.goals.resize(4);
  poses.goals[0].pose.position.x = 0.0;
  poses.goals[0].pose.position.y = 0.0;

  poses.goals[1].pose.position.x = 0.5;
  poses.goals[1].pose.position.y = 0.0;

  poses.goals[2].pose.position.x = 1.0;
  poses.goals[2].pose.position.y = 0.0;

  poses.goals[3].pose.position.x = 2.0;
  poses.goals[3].pose.position.y = 0.0;

  config_->blackboard->set("goals", poses);

  // create waypoint_statuses and set it on blackboard
  std::vector<nav2_msgs::msg::WaypointStatus> waypoint_statuses(poses.goals.size());
  for (size_t i = 0 ; i < waypoint_statuses.size() ; ++i) {
    waypoint_statuses[i].waypoint_pose = poses.goals[i];
    waypoint_statuses[i].waypoint_index = i;
  }
  config_->blackboard->set("waypoint_statuses", waypoint_statuses);

  // tick until node succeeds
  while (tree_->rootNode()->status() != BT::NodeStatus::SUCCESS) {
    tree_->rootNode()->executeTick();
  }

  // check that it removed the point in range
  nav_msgs::msg::Goals output_poses;
  EXPECT_TRUE(config_->blackboard->get("goals", output_poses));

  EXPECT_EQ(output_poses.goals.size(), 2u);
  EXPECT_EQ(output_poses.goals[0], poses.goals[2]);
  EXPECT_EQ(output_poses.goals[1], poses.goals[3]);

  // check the waypoint_statuses
  std::vector<nav2_msgs::msg::WaypointStatus> output_waypoint_statuses;
  EXPECT_TRUE(config_->blackboard->get("waypoint_statuses", output_waypoint_statuses));
  EXPECT_EQ(output_waypoint_statuses.size(), 4u);
  EXPECT_EQ(output_waypoint_statuses[0].waypoint_status, nav2_msgs::msg::WaypointStatus::COMPLETED);
  EXPECT_EQ(output_waypoint_statuses[1].waypoint_status, nav2_msgs::msg::WaypointStatus::COMPLETED);
  EXPECT_EQ(output_waypoint_statuses[2].waypoint_status, nav2_msgs::msg::WaypointStatus::PENDING);
  EXPECT_EQ(output_waypoint_statuses[3].waypoint_status, nav2_msgs::msg::WaypointStatus::PENDING);
}

TEST_F(RemovePassedGoalsTestFixture,
  test_tick_remove_passed_goals_find_matching_waypoint_fail)
{
  geometry_msgs::msg::Pose pose;
  pose.position.x = 0.25;
  pose.position.y = 0.0;

  transform_handler_->waitForTransform();
  transform_handler_->updateRobotPose(pose);

  // create tree
  std::string xml_txt =
    R"(
      <root BTCPP_format="4">
        <BehaviorTree ID="MainTree">
          <RemovePassedGoals radius="0.5" input_goals="{goals}" output_goals="{goals}"
                             input_waypoint_statuses="{waypoint_statuses}"
                             output_waypoint_statuses="{waypoint_statuses}"/>
        </BehaviorTree>
      </root>)";

  tree_ = std::make_shared<BT::Tree>(factory_->createTreeFromText(xml_txt, config_->blackboard));

  // create new goal and set it on blackboard
  nav_msgs::msg::Goals poses;
  poses.goals.resize(4);
  poses.goals[0].pose.position.x = 0.0;
  poses.goals[0].pose.position.y = 0.0;

  poses.goals[1].pose.position.x = 0.5;
  poses.goals[1].pose.position.y = 0.0;

  poses.goals[2].pose.position.x = 1.0;
  poses.goals[2].pose.position.y = 0.0;

  poses.goals[3].pose.position.x = 2.0;
  poses.goals[3].pose.position.y = 0.0;

  config_->blackboard->set("goals", poses);

  // create waypoint_statuses and set it on blackboard
  std::vector<nav2_msgs::msg::WaypointStatus> waypoint_statuses(poses.goals.size());
  for (size_t i = 0 ; i < waypoint_statuses.size() ; ++i) {
    waypoint_statuses[i].waypoint_pose = poses.goals[i];
    waypoint_statuses[i].waypoint_index = i;
  }
  // inconsistency between waypoint_statuses and poses
  waypoint_statuses[1].waypoint_pose.pose.position.x = 0.0;
  config_->blackboard->set("waypoint_statuses", waypoint_statuses);

  // tick until node is not running
  tree_->rootNode()->executeTick();
  while (tree_->rootNode()->status() == BT::NodeStatus::RUNNING) {
    tree_->rootNode()->executeTick();
  }

  // check that it failed
  EXPECT_EQ(tree_->rootNode()->status(), BT::NodeStatus::FAILURE);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  // initialize ROS
  rclcpp::init(argc, argv);

  int all_successful = RUN_ALL_TESTS();

  // shutdown ROS
  rclcpp::shutdown();

  return all_successful;
}
