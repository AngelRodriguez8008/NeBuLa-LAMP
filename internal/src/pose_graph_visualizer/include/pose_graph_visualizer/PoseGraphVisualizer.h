#ifndef POSE_GRAPH_VISUALIZER_H
#define POSE_GRAPH_VISUALIZER_H

#include <unordered_map>

#include <ros/ros.h>

#include <geometry_utils/GeometryUtilsROS.h>

#include <pose_graph_msgs/KeyedScan.h>
#include <pose_graph_msgs/PoseGraph.h>
#include <pose_graph_msgs/PoseGraphEdge.h>
#include <pose_graph_msgs/PoseGraphNode.h>

#include <pose_graph_visualizer/HighlightEdge.h>
#include <pose_graph_visualizer/HighlightNode.h>

#include <core_msgs/Artifact.h>

#include <pcl_ros/point_cloud.h>

#include <tf/transform_datatypes.h>

#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Rot3.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/nonlinear/ISAM2.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/DoglegOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/slam/PriorFactor.h>
#include <gtsam/slam/BetweenFactor.h>
#include <gtsam/slam/InitializePose3.h>
#include <gtsam/nonlinear/NonlinearConjugateGradientOptimizer.h>
#include <gtsam/inference/Symbol.h>

#include <map>
#include <vector>


namespace gu = geometry_utils;

class PoseGraphVisualizer {
public:
  PoseGraphVisualizer() = default;
  ~PoseGraphVisualizer() = default;

  bool Initialize(const ros::NodeHandle &nh, const ros::NodeHandle &pnh);

  // Typedef for stored point clouds.
  typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;

  void MakeMenuMarker(const tf::Pose &position, const std::string &id_number);

  // Visualizes an edge between the two keys.
  bool HighlightEdge(unsigned int key1, unsigned int key2);
  // Removes the edge visualization between the two keys.
  // Removes all highlighting visualizations if both keys are zero.
  void UnhighlightEdge(unsigned int key1, unsigned int key2);

  // Highlights factor graph node associated with the given key.
  bool HighlightNode(unsigned int key);
  // Unhighlights factor graph node associated with the given key.
  // Removes all highlighting visualizations if the key is zero.
  void UnhighlightNode(unsigned int key);

  void VisualizePoseGraph();
  void VisualizeArtifacts();

private:
  bool LoadParameters(const ros::NodeHandle &n);
  bool RegisterCallbacks(const ros::NodeHandle &nh, const ros::NodeHandle &pnh);

  void KeyedScanCallback(const pose_graph_msgs::KeyedScan::ConstPtr &msg);
  void PoseGraphCallback(const pose_graph_msgs::PoseGraph::ConstPtr &msg);
  void
  PoseGraphNodeCallback(const pose_graph_msgs::PoseGraphNode::ConstPtr &msg);
  void
  PoseGraphEdgeCallback(const pose_graph_msgs::PoseGraphEdge::ConstPtr &msg);
  void ArtifactCallback(const core_msgs::Artifact &msg);

  bool
  HighlightNodeService(pose_graph_visualizer::HighlightNodeRequest &request,
                       pose_graph_visualizer::HighlightNodeResponse &response);
  bool
  HighlightEdgeService(pose_graph_visualizer::HighlightEdgeRequest &request,
                       pose_graph_visualizer::HighlightEdgeResponse &response);

  geometry_msgs::Point GetPositionMsg(unsigned int key, const std::map<unsigned int, tf::Pose> &poses) const;

  inline bool KeyExists(unsigned int key) const {
    return keyed_poses_.find(key) != keyed_poses_.end();
  }

  gtsam::Key GetKeyAtTime(const ros::Time &stamp) const;
  gu::Transform3 GetPoseAtKey(const gtsam::Key &key) const;

  // Node name.
  std::string name_;

  // Keep a list of keyed laser scans, poses and timestamps.
  std::map<unsigned int, PointCloud::ConstPtr> keyed_scans_;
  std::map<unsigned int, tf::Pose> keyed_poses_;
  std::map<unsigned int, tf::Pose> keyed_artifact_poses_;
  std::map<unsigned int, tf::Pose> keyed_uwb_poses_;
  std::map<unsigned int, ros::Time> keyed_stamps_;
  std::map<double, unsigned int> stamps_keyed_;

  // Frames.
  std::string fixed_frame_id_;
  std::string base_frame_id_;
  bool artifacts_in_global_;

  // Artifacts and labels.
  struct ArtifactInfo {
    // gtsam::Key pose_key;
    core_msgs::Artifact msg;
  };
  std::unordered_map<gtsam::Key, ArtifactInfo> artifacts_;
  int largest_artifact_id_{0};
  std::unordered_map<std::string, gtsam::Key> artifact_id2key_hash_;
  Eigen::Vector3d GetArtifactPosition(const gtsam::Key artifact_key) const;

  // Visualization publishers.
  ros::Publisher odometry_edge_pub_;
  ros::Publisher loop_edge_pub_;
  ros::Publisher artifact_edge_pub_;
  ros::Publisher uwb_edge_pub_;
  ros::Publisher uwb_node_pub_;
  ros::Publisher graph_node_pub_;
  ros::Publisher graph_node_id_pub_;
  ros::Publisher keyframe_node_pub_;
  ros::Publisher closure_area_pub_;
  ros::Publisher highlight_pub_;
  ros::Publisher artifact_marker_pub_;

  // Subscribers.
  ros::Subscriber keyed_scan_sub_;
  ros::Subscriber pose_graph_sub_;
  ros::Subscriber pose_graph_node_sub_;
  ros::Subscriber pose_graph_edge_sub_;
  ros::Subscriber artifact_sub_;

  // Services.
  ros::ServiceServer highlight_node_srv_;
  ros::ServiceServer highlight_edge_srv_;

  typedef std::pair<unsigned int, unsigned int> Edge;
  std::vector<Edge> odometry_edges_;
  std::vector<Edge> loop_edges_;
  std::vector<Edge> artifact_edges_;
  std::vector<Edge> uwb_edges_;

  bool publish_interactive_markers_{true};

  // Proximity threshold used by LaserLoopClosureNode.
  double proximity_threshold_{1};

  unsigned int key_{0};
};

#endif
