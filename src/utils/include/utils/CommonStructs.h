/*
 * Copyright Notes
 *
 * Authors:
 * Alex Stephens       (alex.stephens@jpl.nasa.gov)
 * Benjamin Morrell    (benjamin.morrell@jpl.nasa.gov)
 */

#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include <boost/function.hpp>

// GTSAM
#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Rot3.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/slam/BetweenFactor.h>
#include <gtsam/slam/InitializePose3.h>
#include <gtsam/slam/PriorFactor.h>

#include <geometry_utils/GeometryUtilsROS.h>
#include <geometry_utils/Transform3.h>

#include <pcl_ros/point_cloud.h>

#include <geometry_msgs/PoseStamped.h>
#include <pose_graph_msgs/PoseGraph.h>
#include <pose_graph_msgs/PoseGraphEdge.h>
#include <pose_graph_msgs/PoseGraphNode.h>

// Typedef for 6x6 covariance matrices (x, y, z, roll, pitch, yaw).
typedef geometry_utils::MatrixNxNBase<double, 6> Mat66;
typedef geometry_utils::MatrixNxNBase<double, 12> Mat1212;

// Noise models
typedef gtsam::noiseModel::Gaussian Gaussian;
typedef gtsam::noiseModel::Diagonal Diagonal;

// GTSAM edge types
typedef std::pair<gtsam::Symbol, gtsam::Symbol> Edge;
typedef std::pair<gtsam::Symbol, gtsam::Symbol> ArtifactEdge;
typedef std::pair<gtsam::Symbol, gtsam::Pose3> Prior;

typedef pose_graph_msgs::PoseGraphNode NodeMessage;
typedef pose_graph_msgs::PoseGraphEdge EdgeMessage;
typedef pose_graph_msgs::PoseGraph::ConstPtr GraphMsgPtr;

typedef std::vector<EdgeMessage> EdgeMessages;
typedef std::vector<NodeMessage> NodeMessages;

// Typedef for stored point clouds.
typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;

// Function that maps gtsam::Symbol to internal identifier string.
typedef boost::function<std::string(gtsam::Symbol)> SymbolIdMapping;

// Forward declaration.
class PoseGraph;

// GTSAM factor (edge).
struct Factor {
  gtsam::Symbol key_from;
  gtsam::Symbol key_to;
  int type;
  gtsam::Pose3 transform;
  gtsam::SharedNoiseModel covariance;

  // Optional pointer to parent pose graph.
  PoseGraph* graph{nullptr};

  EdgeMessage ToMsg() const;
  static Factor FromMsg(const EdgeMessage& msg);
};

// GTSAM node (prior).
struct Node {
  ros::Time stamp;
  std::string fixed_frame_id;
  gtsam::Symbol key;
  // Type-dependent ID that is optionally set.
  std::string ID{""};
  gtsam::Pose3 pose;
  gtsam::SharedNoiseModel covariance;

  // Optional pointer to parent pose graph.
  PoseGraph* graph{nullptr};

  NodeMessage ToMsg() const;
  static Node FromMsg(const NodeMessage& msg);

  Node(const ros::Time& stamp,
       gtsam::Symbol key,
       const gtsam::Pose3& pose,
       const gtsam::SharedNoiseModel& covariance,
       PoseGraph* graph = nullptr);
};

// Pose graph structure storing values, factors and meta data.
class PoseGraph {
public:
  gtsam::Values values;
  gtsam::NonlinearFactorGraph nfg;

  // Function that maps gtsam::Symbol to std::string (internal identifier for
  // node messages).
  SymbolIdMapping symbol_id_map;

  std::string fixed_frame_id;

  // Keep a list of keyed laser scans and keyed timestamps.
  std::map<gtsam::Symbol, PointCloud::ConstPtr> keyed_scans;
  std::map<gtsam::Symbol, ros::Time> keyed_stamps; // All nodes
  std::map<double, gtsam::Symbol> stamp_to_odom_key;

  // Message filters (if any)
  std::string prefix{""};

  // Initial key
  gtsam::Symbol initial_key{0};

  // Current key
  gtsam::Symbol key;

  gtsam::Vector6 initial_noise{gtsam::Vector6::Zero()};

  inline gtsam::Pose3 LastPose() const {
    return values.at<gtsam::Pose3>(key - 1);
  }
  inline gtsam::Pose3 GetPose(gtsam::Symbol key) const {
    return values.at<gtsam::Pose3>(key);
  }

  void Initialize(gtsam::Symbol initial_key,
                  const gtsam::Pose3& pose,
                  const Diagonal::shared_ptr& covariance);

  void TrackFactor(const Factor& factor);
  void TrackFactor(const EdgeMessage& msg);
  void TrackFactor(gtsam::Symbol key_from,
                   gtsam::Symbol key_to,
                   int type,
                   const gtsam::Pose3& transform,
                   const gtsam::SharedNoiseModel& covariance);
  void TrackNode(const Node& node);
  void TrackNode(const NodeMessage& msg);
  void TrackNode(const ros::Time& stamp,
                 gtsam::Symbol key,
                 const gtsam::Pose3& pose,
                 const gtsam::SharedNoiseModel& covariance);

  void AddNewValues(const gtsam::Values& new_values);

  // Time threshold for time-based lookup functions.
  static double time_threshold;
  // Convert timestamps to gtsam keys.
  gtsam::Symbol GetKeyAtTime(const ros::Time& stamp) const;
  gtsam::Symbol GetClosestKeyAtTime(const ros::Time& stamp) const;
  inline static bool IsTimeWithinThreshold(double time,
                                           const ros::Time& target) {
    return std::abs(time - target.toSec()) <= time_threshold;
  }
  // Check if given key has a registered time stamp.
  inline bool HasTime(gtsam::Symbol key) const {
    return keyed_stamps.find(key) != keyed_stamps.end();
  }

  // Saves pose graph and accompanying point clouds to a zip file.
  template <typename PGOSolver>
  bool Save(const std::string& zipFilename, PGOSolver& solver) const;

  // Loads pose graph and accompanying point clouds from a zip file.
  template <typename PGOSolver>
  bool Load(const std::string& zipFilename, PGOSolver& solver);

  // Convert entire pose graph to message.
  GraphMsgPtr ToMsg() const;

  // Generates message from factors and values that were modified since the
  // last update.
  GraphMsgPtr ToIncrementalMsg() const;

  // Incremental update from pose graph message.
  void UpdateFromMsg(const GraphMsgPtr& msg);

  inline void ClearIncrementalMessages() {
    edges_new_.clear();
    priors_new_.clear();
    values_new_.clear();
  }

  inline const EdgeMessages& GetEdges() const {
    return edges_;
  }
  inline const NodeMessages& GetPriors() const {
    return priors_;
  }

private:
  // Cached messages for edges and priors to reduce publishing overhead.
  EdgeMessages edges_;
  NodeMessages priors_;

  // Variables for tracking the new features only
  gtsam::Values values_new_;
  EdgeMessages edges_new_;
  NodeMessages priors_new_;

  // Convert incremental pose graph with given values, edges and priors to
  // message.
  GraphMsgPtr ToMsg_(const gtsam::Values& values,
                     const EdgeMessages& edges,
                     const NodeMessages& priors) const;
};

// Struct definition
struct FactorData {
  bool b_has_data;  // False if there is no data
  std::string type; // odom, artifact, loop clsoure
  // Vector for possible multiple factors
  std::vector<gtsam::Pose3> transforms; // The transform (for odom, loop
                                        // closures etc.) and pose for TS
  std::vector<gtsam::SharedNoiseModel>
      covariances; // Covariances for each transform
  std::vector<std::pair<ros::Time, ros::Time>>
      time_stamps; // Time when the measurement as acquired (first, second)
  // TODO - use ros::Time or something else?

  std::vector<gtsam::Symbol> artifact_key; // key for the artifacts
};

#endif

// need to include source file for templatized save/load functions
#include "utils/PoseGraphFileIO.hpp"
