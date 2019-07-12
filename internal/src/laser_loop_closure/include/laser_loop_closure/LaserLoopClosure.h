/*
 * Copyright (c) 2016, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Author: Erik Nelson            ( eanelson@eecs.berkeley.edu )
 */

#ifndef LASER_LOOP_CLOSURE_H
#define LASER_LOOP_CLOSURE_H

// enables correct operations of GTSAM (correct Jacobians)
#define SLOW_BUT_CORRECT_BETWEENFACTOR 

#include <ros/ros.h>
#include <geometry_utils/Matrix3x3.h>
#include <geometry_utils/Transform3.h>
#include <point_cloud_filter/PointCloudFilter.h>
#include <pose_graph_msgs/KeyedScan.h>
#include <pose_graph_msgs/PoseGraph.h>
#include <pose_graph_msgs/PoseGraphEdge.h>
#include <pose_graph_msgs/PoseGraphNode.h>

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

#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2_ros/transform_listener.h>

#include <tf/transform_datatypes.h>

#include <core_msgs/Artifact.h>

// for UWB
#include <gtsam/sam/RangeFactor.h>
#include <gtsam/inference/Symbol.h>

// #include "SESync/SESync.h"
// #include "SESync/SESync_utils.h"

// This new header allows us to read examples easily from .graph files
#include <gtsam/slam/dataset.h>

#include <pcl_ros/point_cloud.h>

#include <map>
#include <vector>

#include "RobustPGO/RobustPGO.h" // RobustPGO (backend solver)
#include "RobustPGO/pcm/pcm.h"

// 1 for LevenbergMarquardt, 2 for GaussNewton, 3 for SESync (WIP)
#define SOLVER 1

struct ArtifactInfo {
  std::string id; // this corresponds to parent_id
  core_msgs::Artifact msg; // All fields in the artifact message that we need
  int num_updates; // how many times the optimizer has updated this
  ArtifactInfo(std::string art_id="") :
               id(art_id), 
               num_updates(0){}
};

class LaserLoopClosure {
 public:
  LaserLoopClosure();
  ~LaserLoopClosure();

  bool Initialize(const ros::NodeHandle& n);

  // Typedef for 6x6 covariance matrices (x, y, z, roll, pitch, yaw).
  typedef geometry_utils::MatrixNxNBase<double, 6> Mat66;
  typedef geometry_utils::MatrixNxNBase<double, 12> Mat1212;

  // Typedef for stored point clouds.
  typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;

  // Call this every time the robot's pose has been updated via ICP or some
  // other form of odometry. A between factor will always be added, but this
  // function will only return true when the new pose is significantly
  // different from the most recently added pose to enforce graph sparsity.
  // A return value of true lets the caller know when they should call
  // AddKeyScanPair().
  bool AddBetweenFactor(const geometry_utils::Transform3& delta,
                        const Mat66& covariance, const ros::Time& stamp,
                        gtsam::Symbol* key);
  
  bool AddUwbFactor(const std::string uwb_id,
                    const ros::Time& stamp,
                    const double range,
                    const Eigen::Vector3d robot_position);
  
  bool DropUwbAnchor(const std::string uwb_id,
                     const ros::Time& stamp,
                     const Eigen::Vector3d robot_position);

  // Upon successful addition of a new between factor, call this function to
  // associate a laser scan with the new pose.
  bool AddKeyScanPair(gtsam::Symbol key, const PointCloud::ConstPtr& scan, bool initial_pose);

  // After receiving an output key from 'AddBetweenFactor', call this to check
  // for loop closures with other poses in the pose graph.
  bool FindLoopClosures(gtsam::Symbol key,
                        std::vector<gtsam::Symbol>* closure_keys);

  // Build a 3D point cloud by concatenating all point clouds from poses along
  // the pose graph.
  bool GetMaximumLikelihoodPoints(PointCloud* map);

  // Get the most recent pose in the pose graph.
  geometry_utils::Transform3 GetLastPose() const;
  gtsam::Symbol GetKey() const;

  //Get initial key
   gtsam::Symbol GetInitialKey() const;

  bool AddFactorAtRestart(const geometry_utils::Transform3& delta,const LaserLoopClosure::Mat66& covariance);
  bool AddFactorAtLoad(const geometry_utils::Transform3& delta,const LaserLoopClosure::Mat66& covariance);

  // Get the most initial pose in the pose graph.
  geometry_utils::Transform3 GetInitialPose() const;

  // Get pose at an input time
  gtsam::Key GetKeyAtTime(const ros::Time& stamp) const;

  // Get pose at an input key 
  geometry_utils::Transform3 GetPoseAtKey(const gtsam::Key& key) const; 

  Eigen::Vector3d GetArtifactPosition(const gtsam::Key artifact_key) const;

  // Publish pose graph for visualization.
  bool PublishPoseGraph(bool only_publish_if_changed = true);

  // Publish artifacts for visualization. 
  void PublishArtifacts(gtsam::Key artifact_key = gtsam::Key(gtsam::Symbol('z',0)));

  // Changes the keynumber of key_
  bool ChangeKeyNumber();

  // Function to search for loopclosures over the whole posegraph
  bool BatchLoopClosure();

  // AddManualLoopClosure between the two keys to connect them. This function is
  // designed for a scenario where a human operator can manually perform
  // loop closures by adding these factors to the pose graph.
  bool AddManualLoopClosure(gtsam::Key key1, gtsam::Key key2, gtsam::Pose3 pose12);

  bool AddArtifact(gtsam::Key posekey, gtsam::Key artifact_key, gtsam::Pose3 pose12,
                   ArtifactInfo artifact);

  bool AddFactor(gtsam::Key key1, gtsam::Key key2, 
                 gtsam::Pose3 pose12, 
                 bool is_manual_loop_closure,
                 double rot_precision, 
                 double trans_precision);

  // Removes the factor between the two keys from the pose graph.
  bool RemoveFactor(gtsam::Symbol key1,
                    gtsam::Symbol key2,
                    bool is_batch_loop_closure = false);

  // Erase the posegraph
  bool ErasePosegraph();

  //Test to not add laserloopclosures close to a manual loop closure
  bool BatchLoopClosingTest(unsigned int key, unsigned int other_key);

  // Saves pose graph and accompanying point clouds to a zip file.
  bool Save(const std::string &zipFilename) const;

  // Loads pose graph and accompanying point clouds from a zip file.
  bool Load(const std::string &zipFilename);

  //Basestation callbackfunctions
  void KeyedScanBaseHandler(const pose_graph_msgs::KeyedScan::ConstPtr& msg);
  void PoseGraphBaseHandler(const pose_graph_msgs::PoseGraph::ConstPtr& msg);

private:
  bool LoadParameters(const ros::NodeHandle& n);
  bool RegisterCallbacks(const ros::NodeHandle& n);

  // Checks on loop closure 
  bool SanityCheckForLoopClosure(double translational_sanity_check, double cost_old, double cost);

  // Pose conversion from/to GTSAM format.
  geometry_utils::Transform3 ToGu(const gtsam::Pose3& pose) const;
  gtsam::Pose3 ToGtsam(const geometry_utils::Transform3& pose) const;

  // Covariance conversion from/to GTSAM format.
  typedef gtsam::noiseModel::Gaussian Gaussian;
  typedef gtsam::noiseModel::Diagonal Diagonal;
  Mat66 ToGu(const Gaussian::shared_ptr& covariance) const;
  Gaussian::shared_ptr ToGtsam(const Mat66& covariance) const;
  Gaussian::shared_ptr ToGtsam(const Mat1212& covariance) const;

  // Diagonal of the covariance matrix of the first pose
  gtsam::Vector6 initial_noise_;

  // Create prior and between factors.
  gtsam::PriorFactor<gtsam::Pose3> MakePriorFactor(
      const gtsam::Pose3& pose, const Diagonal::shared_ptr& covariance);
  gtsam::BetweenFactor<gtsam::Pose3> MakeBetweenFactor(
      const gtsam::Pose3& pose, const Gaussian::shared_ptr& covariance);
  gtsam::BetweenFactor<gtsam::Pose3> MakeBetweenFactorAtLoad(
      const gtsam::Pose3& pose, const Gaussian::shared_ptr& covariance);

  // Perform ICP between two laser scans.
  /**
   *  Is the query scan filtered and transformed. Decreases computation of filtering the same query scan
   *multiple times when matching to different scans. 
   *
   * @param[in]: is_filtered -->  is the scan already filtered
   * @param[in]: frame_id  -->    Coordinate frame of the scan. ICP converts the frame to world, so currently would be just focussing on making changes for world.
   */  
  bool PerformICP(PointCloud::Ptr& scan1,
                  const PointCloud::ConstPtr& scan2,
                  const geometry_utils::Transform3& pose1,
                  const geometry_utils::Transform3& pose2,
                  geometry_utils::Transform3* delta, Mat66* covariance, const bool is_filtered, const std::string frame_id);

  // Perform ICP between two laser scans.
  /**
   *  Is the query scan filtered and transformed. Decreases computation of filtering the same query scan
   *multiple times when matching to different scans. 
   *
   * @param[in]: is_filtered -->  is the scan already filtered
   * @param[in]: frame_id  -->    Coordinate frame of the scan. ICP converts the frame to world, so currently would be just focussing on making changes for world.
   */
  bool PerformICP(PointCloud::Ptr& scan1,
                  const PointCloud::ConstPtr& scan2,
                  const geometry_utils::Transform3& pose1,
                  const geometry_utils::Transform3& pose2,
                  geometry_utils::Transform3* delta, Mat1212* covarianc, const bool is_filtered, const std::string frame_id);

  // bool AddFactorService(laser_loop_closure::ManualLoopClosureRequest &request,
  //                       laser_loop_closure::ManualLoopClosureResponse &response);

  // Node name.
  std::string name_;

  // Keep a list of keyed laser scans and keyed timestamps.
  std::map<gtsam::Symbol, PointCloud::ConstPtr> keyed_scans_;
  std::map<gtsam::Symbol, ros::Time> keyed_stamps_;
  std::map<double, gtsam::Symbol> stamps_keyed_;

  // Aggregate odometry until we can update the pose graph.
  gtsam::Pose3 odometry_;
  gtsam::Pose3 odometry_kf_;

  // Pose graph and ISAM2 parameters.
  bool check_for_loop_closures_;
  bool save_posegraph_backup_;
  bool LAMP_recovery_;
  unsigned int keys_between_each_posegraph_backup_;
  gtsam::Symbol key_;
  gtsam::Symbol last_closure_key_;
  unsigned int relinearize_interval_;
  double distance_to_skip_recent_poses_;
  unsigned int skip_recent_poses_;
  double distance_before_reclosing_;
  unsigned int poses_before_reclosing_;
  unsigned int n_iterations_manual_loop_close_;
  double translation_threshold_nodes_;
  double rotation_threshold_nodes_;
  double translation_threshold_kf_;
  double proximity_threshold_;
  double max_tolerable_fitness_;
  double manual_lc_rot_precision_;
  double manual_lc_trans_precision_;
  double artifact_rot_precision_;
  double artifact_trans_precision_;
  double laser_lc_rot_sigma_;
  double laser_lc_trans_sigma_;
  unsigned int relinearize_skip_;
  double relinearize_threshold_;
  bool publish_interactive_markers_;
  std::vector<unsigned int> manual_loop_keys_;
  double odom_threshold_;
  double pw_threshold_;
  gtsam::Symbol initial_key_;
   gtsam::Symbol artifact_key_;
  gtsam::Symbol first_loaded_key_;
  gtsam::Symbol stored_key_;

  //Basestation
  bool b_is_basestation_;
  bool b_first_key_set_;

  // Sanity check parameters
  bool b_check_deltas_; 
  double translational_sanity_check_lc_;
  double translational_sanity_check_odom_;

  // ICP parameters.
  double icp_ransac_thresh_;
  double icp_tf_epsilon_;
  double icp_corr_dist_;
  unsigned int icp_iterations_;
  geometry_utils::Transform3 delta_icp_;

  // UWB parameters
  std::unordered_map<std::string, gtsam::Key> uwb_id2key_hash_;
  std::unordered_map<gtsam::Key, std::string> uwb_key2id_hash_;
  double uwb_range_measurement_error_;
  unsigned int uwb_range_compensation_;

  // Optimizer object, and best guess pose values.
  std::unique_ptr<RobustPGO> pgo_solver_;

  gtsam::NonlinearFactorGraph nfg_;
  gtsam::PriorFactor<gtsam::Pose3> prior_factor_;
  gtsam::Values values_;

  // Backup values
  gtsam::NonlinearFactorGraph nfg_backup_;
  gtsam::Values values_backup_;

  // Frames.
  std::string fixed_frame_id_;
  std::string base_frame_id_;

  // Artifacts and labels 
  std::unordered_map<gtsam::Key, ArtifactInfo> artifact_key2info_hash;

  // Visualization publishers.
  ros::Publisher scan1_pub_;
  ros::Publisher scan2_pub_;
  ros::Publisher artifact_pub_;
  ros::Publisher erase_posegraph_pub_;
  ros::Publisher remove_factor_viz_pub_;

  std::map<long unsigned int, tf::Pose> keyed_poses_;
  
  //Function to get the gu position of all the keys
  geometry_utils::Transform3 GetPoseAtLoadedKey(const gtsam::Key &key) const;

  // Used for publishing pose graph only if it hasn't changed.
  bool has_changed_{true};

  // ros::ServiceServer add_factor_srv_;

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;

  // Pose graph publishers.
  ros::Publisher pose_graph_pub_;
  ros::Publisher keyed_scan_pub_;
  ros::Publisher loop_closure_notifier_pub_;

  typedef std::pair<gtsam::Symbol, gtsam::Symbol> Edge;
  typedef std::pair<gtsam::Symbol, gtsam::Symbol> ArtifactEdge;
  std::vector<Edge> odometry_edges_;
  std::vector<Edge> loop_edges_;
  std::vector<Edge> manual_loop_edges_;
  std::vector<ArtifactEdge> artifact_edges_;
  std::vector<std::pair<unsigned int, gtsam::Key>> uwb_edges_;
  std::map<Edge, gtsam::Pose3> edge_poses_;
  std::map<Edge, LaserLoopClosure::Mat66> covariance_betweenfactor_;

  // For filtering laser scans prior to ICP.
  PointCloudFilter filter_;
};

#endif
