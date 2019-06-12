/* 
Generic solver class 
author: Yun Chang, Luca Carlone
*/

#include <ros/ros.h>

// enables correct operations of GTSAM (correct Jacobians)
#define SLOW_BUT_CORRECT_BETWEENFACTOR 

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

#include "graph_utils/graph_utils_functions.h" // from distriuted mapper

class GenericSolver {
public:
  GenericSolver(int solvertype=1); 
  // solvertype = 1 for LevenbergMarquardt, 2 for GaussNewton, 3 for SESync (WIP)

  void regularUpdate(gtsam::NonlinearFactorGraph nfg=gtsam::NonlinearFactorGraph(), 
              gtsam::Values values=gtsam::Values(),
              gtsam::FactorIndices factorsToRemove=gtsam::FactorIndices());

  void update(gtsam::NonlinearFactorGraph nfg=gtsam::NonlinearFactorGraph(), 
                    gtsam::Values values=gtsam::Values(),
                    gtsam::FactorIndices factorsToRemove=gtsam::FactorIndices());

  gtsam::Values calculateEstimate() { return values_; }
  gtsam::Values calculateBestEstimate() { return values_; }
  gtsam::Values getLinearizationPoint() { return values_; }
  gtsam::NonlinearFactorGraph getFactorsUnsafe(){ return nfg_; }
  bool LoadParameters();

  void print() {
    nfg_.print("");
    values_.print("");
  }

private:
  gtsam::Values values_;
  gtsam::NonlinearFactorGraph nfg_;
  int solver_type_;

  double odom_threshold_;
  double pw_threshold_;

  gtsam::NonlinearFactorGraph nfg_odom_;
  gtsam::NonlinearFactorGraph nfg_lc_;
  gtsam::Matrix lc_adjacency_matrix_;
  graph_utils::Trajectory posesAndCovariances_odom_; 

  void initializePrior(gtsam::PriorFactor<gtsam::Pose3> prior_factor);

  void updateOdom(gtsam::BetweenFactor<gtsam::Pose3> odom_factor, 
                  graph_utils::PoseWithCovariance &new_pose);

  bool isOdomConsistent(gtsam::BetweenFactor<gtsam::Pose3> lc_factor);

  bool areLoopsConsistent(gtsam::BetweenFactor<gtsam::Pose3> lc_1, 
                          gtsam::BetweenFactor<gtsam::Pose3> lc_2);

  void findInliers(gtsam::NonlinearFactorGraph &inliers);
};