/*
 * Copyright Notes
 *
 * Authors: Benjamin Morrell    (benjamin.morrell@jpl.nasa.gov)
 */


// Includes
#include <lamp/LampBase.h>

// #include <math.h>
// #include <ctime>

namespace pu = parameter_utils;
namespace gu = geometry_utils;
namespace gr = gu::ros;

using gtsam::BetweenFactor;
using gtsam::RangeFactor;
using gtsam::NonlinearFactorGraph;
using gtsam::Pose3;
using gtsam::PriorFactor;
using gtsam::Rot3;
using gtsam::Values;
using gtsam::Vector3;

// Constructor
LampBase::LampBase():
    prefix_("") {
     // any other things on construction 
    }

// Destructor
LampBase::~LampBase() {}

// Initialization
bool LampBase::Initialize(const ros::NodeHandle& n) {

  LoadParameters(n);
  CreatePublishers(n);
  // InitializeHandlers(n);

}

// Load Parameters
bool LampBase::LoadParameters(const ros::NodeHandle& n) {

}

// Create Publishers
bool LampBase::CreatePublishers(const ros::NodeHandle& n) {
  ros::NodeHandle nl(n);
  pose_graph_pub_ =
      nl.advertise<pose_graph_msgs::PoseGraph>("pose_graph", 10, false);
}

// bool LampBase::RegisterCallbacks(const ros::NodeHandle& n){
//   ros::NodeHandle nl(n);

//   return false;
// }

bool LampBase::InitializeHandlers(const ros::NodeHandle& n){
  return false;
}

bool LampBase::CheckHandlers(){
  return false;
}

gtsam::Key LampBase::getKeyAtTime(const ros::Time& stamp) const {

  auto iterTime = stamp_to_odom_key_.lower_bound(stamp.toSec()); // First key that is not less than timestamp 

  // TODO - interpolate - currently just take one
  double t2 = iterTime->first;

  if (iterTime == stamp_to_odom_key_.begin()){
    ROS_WARN("Only one value in the graph - using that");
    return iterTime->second;
  }
  double t1 = std::prev(iterTime,1)->first; 

  // std::cout << "Time 1 is: " << t1 << ", Time 2 is: " << t2 << std::endl;

  gtsam::Symbol key;

  // if (t2-stamp.toSec() < stamp.toSec() - t1) {
  //   // t2 is closer - use that key
  //   // std::cout << "Selecting later time: " << t2 << std::endl;
  //   key = iterTime->second;
  // } else {
  //   // t1 is closer - use that key
  //   // std::cout << "Selecting earlier time: " << t1 << std::endl;
  //   key = std::prev(iterTime,1)->second;
  //   iterTime--;
  // }
  // // std::cout << "Key is: " << key << std::endl;
  // if (iterTime == std::prev(stamp_to_odom_key_.begin())){
  //   ROS_WARN("Invalid time for graph (before start of graph range). Choosing next value");
  //   iterTime++;
  //   // iterTime = stamp_to_odom_key_.begin();
  //   key = iterTime->second;
  // } else if(iterTime == stamp_to_odom_key_.end()) {
  //   ROS_WARN("Invalid time for graph (past end of graph range). take latest pose");
  //   key = key_ -1;
  // }

  return key; 

}

// void LampBase::OptimizerUpdateCallback(const pose_graph_msgs::PoseGraphConstPtr &msg) {
  
//   // Process the slow graph update
//   merger_.on_slow_graph_msg(msg);

//   // Give merger the current graph 
//   merger_.on_fast_graph_msg(ConvertPoseGraphToMsg());

//   gtsam::Values new_values; 
//   gtsam::Symbol key;

//   // Update the internal LAMP graph using the one stored by the merger
//   pose_graph_msgs::PoseGraph fused_graph = merger_.GetCurrentGraph();

//   // update the LAMP internal values_
//   // Function pose_graph msg to gtsam values and factors
//   utils::ConvertMsgToPoseGraph(msg, values_, nfg_);

//   for (const pose_graph_msgs::PoseGraphNode &msg_node : fused_graph.nodes) {
//     // Get key 
//     key = gtsam::Symbol(msg_node.key);

//     gtsam::Pose3 full_pose;

//     gtsam::Point3 pose_translation(msg_node.pose.position.x,
//                                   msg_node.pose.position.y,
//                                   msg_node.pose.position.z);
//     gtsam::Rot3 pose_orientation(gtsam::Rot3::quaternion(msg_node.pose.orientation.w,
//                                                   msg_node.pose.orientation.x,
//                                                   msg_node.pose.orientation.y,
//                                                   msg_node.pose.orientation.z));
//     full_pose = gtsam::Pose3(pose_orientation, pose_translation);

//     new_values.insert(key, full_pose);
//   }

//   // Update internal pose graph values
//   values_ = new_values;
// }


// Function returns a vector of node messages from an input values
bool LampBase::ConvertValuesToNodeMsgs(std::vector<pose_graph_msgs::PoseGraphNode>& nodes){
  // Converts the internal values 

  for (const auto& keyed_pose : values_) {
    if (!values_.exists(keyed_pose.key)) {
      ROS_WARN("Key, %lu, does not exist in PublishPoseGraph pose graph pub", keyed_pose.key);
      return false;
    }

    gu::Transform3 t = utils::ToGu(values_.at<gtsam::Pose3>(keyed_pose.key));

    gtsam::Symbol sym_key = gtsam::Symbol(keyed_pose.key);

    // Populate the message with the pose's data.
    pose_graph_msgs::PoseGraphNode node;
    node.key = keyed_pose.key;
    node.header.frame_id = fixed_frame_id_;
    node.pose = gr::ToRosPose(t);
    
    // Get timestamp
    node.header.stamp = keyed_stamps_[keyed_pose.key];

    // Get the IDs
    if (keyed_scans_.count(keyed_pose.key)) {
      // Key frame, note in the ID
      node.ID = "key_frame";
    }else if (sym_key.chr() == prefix_[0]){
      // Odom or key frame
      node.ID = "odom";
    }else if (sym_key.chr() == 'u'){
      // UWB
      // node.ID = uwd_handler_.GetUWBID(keyed_pose.key); // TODO
      node.ID = "UWB"; // TEMPORARY
    } else {
      // Artifact
      // node.ID = artifact_handler_.GetArtifactID(keyed_pose.key);// TODO
      node.ID = "Artifact"; // TEMPORARY
    }

    // Add to the vector of nodes
    nodes.push_back(node);
  }
  return true;
}

// Convert the internally stored pose_graph to a pose graph message
pose_graph_msgs::PoseGraphConstPtr LampBase::ConvertPoseGraphToMsg(){
  // Uses internally stored info. 

  // Create the Pose Graph Message
  pose_graph_msgs::PoseGraph g;
  g.header.frame_id = fixed_frame_id_;
  g.header.stamp = keyed_stamps_[key_ - 1]; // Get timestamp from latest keyed pose

  // Flag on whether it is incremental or not
  // TODO make incremental Pose Graph publishing
  g.incremental = false;

  // Get Values
  ConvertValuesToNodeMsgs(g.nodes);

  // Add the factors 
  g.edges = edges_info_;
  g.priors = priors_info_;

  pose_graph_msgs::PoseGraphConstPtr g_ptr(new pose_graph_msgs::PoseGraph(g));

  return g_ptr;

}


bool LampBase::PublishPoseGraph(){
  // TODO - publish incremental

  // Convert master pose-graph to messages
  pose_graph_msgs::PoseGraphConstPtr g = ConvertPoseGraphToMsg();

  // Publish 
  pose_graph_pub_.publish(*g);

  return true;

}