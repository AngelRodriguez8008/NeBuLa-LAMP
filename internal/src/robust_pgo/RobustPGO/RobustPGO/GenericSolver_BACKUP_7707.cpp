<<<<<<< HEAD
/* 
Generic solver class 
No outlier removal in this class 
=======
/*
Generic solver class
No outlier removal in this class
>>>>>>> master
author: Yun Chang, Luca Carlone
*/

#include "RobustPGO/GenericSolver.h"

<<<<<<< HEAD
GenericSolver::GenericSolver(int solvertype, 
                             std::vector<char> special_symbols): 
=======
namespace RobustPGO {

GenericSolver::GenericSolver(Solver solvertype,
                             std::vector<char> special_symbols):
>>>>>>> master
  nfg_(gtsam::NonlinearFactorGraph()),
  values_(gtsam::Values()),
  solver_type_(solvertype),
  special_symbols_(special_symbols),
<<<<<<< HEAD
  debug_(true), 
  save_g2o_(false) {}

bool GenericSolver::specialSymbol(char symb) {
  for (size_t i = 0; i < special_symbols_.size(); i++) {
    if (special_symbols_[i] == symb) return true;
  }
  return false; 
}

void GenericSolver::update(gtsam::NonlinearFactorGraph nfg, 
                           gtsam::Values values, 
                           gtsam::FactorIndices factorsToRemove) {
  // remove factors
  for (size_t index : factorsToRemove) {
    nfg_[index].reset();
  }

  // add new values and factors
  nfg_.add(nfg);
  values_.insert(values);
  bool do_optimize = true; 

  // Do not optimize for just odometry additions 
  // odometry values would not have prefix 'l' unlike artifact values
  if (nfg.size() == 1 && values.size() == 1) {
    const gtsam::Symbol symb(values.keys()[0]); 
    if (!specialSymbol(symb.chr())) {do_optimize = false;}
  }

  // nothing added so no optimization 
  if (nfg.size() == 0 && values.size() == 0) {do_optimize = false;}

  if (factorsToRemove.size() > 0) 
    do_optimize = true;

  if (do_optimize) {
    // optimize
    if (solver_type_ == 1) {
      gtsam::LevenbergMarquardtParams params;
      if (debug_) {
        params.setVerbosityLM("SUMMARY");
        log<INFO>("Running LM"); 
      }
      params.diagonalDamping = true; 
      values_ = gtsam::LevenbergMarquardtOptimizer(nfg_, values_, params).optimize();
    }else if (solver_type_ == 2) {
=======
  debug_(true) {}

bool GenericSolver::isSpecialSymbol(char symb) const {
  for (size_t i = 0; i < special_symbols_.size(); i++) {
    if (special_symbols_[i] == symb) return true;
  }
  return false;
}

bool GenericSolver::addAndCheckIfOptimize(const gtsam::NonlinearFactorGraph& nfg,
      const gtsam::Values& values) {
  // add new values and factors
  nfg_.add(nfg);
  values_.insert(values);
  bool do_optimize = true;

  // Do not optimize for just odometry additions
  // odometry
  if (nfg.size() == 1 && values.size() == 1) {return false;}

  // nothing added so no optimization
  if (nfg.size() == 0 && values.size() == 0) {return false;}

  return true;
}

void GenericSolver::update(const gtsam::NonlinearFactorGraph& nfg,
                           const gtsam::Values& values,
                           const gtsam::FactorIndices& factorsToRemove) {
  // remove factors
  bool remove_factors = false;
  if (factorsToRemove.size() > 0) {remove_factors = true;}
  for (size_t index : factorsToRemove) {
    nfg_[index].reset();
  }

  bool process_lc = addAndCheckIfOptimize(nfg, values);

  if (process_lc || remove_factors) {
    // optimize
    if (solver_type_ == Solver::LM) {
      gtsam::LevenbergMarquardtParams params;
      if (debug_) {
        params.setVerbosityLM("SUMMARY");
        log<INFO>("Running LM");
      }
      params.diagonalDamping = true;
      values_ = gtsam::LevenbergMarquardtOptimizer(nfg_, values_, params).optimize();
    }else if (solver_type_ == Solver::GN) {
>>>>>>> master
      gtsam::GaussNewtonParams params;
      if (debug_){
        params.setVerbosity("ERROR");
        log<INFO>("Running GN");
      }
      values_ = gtsam::GaussNewtonOptimizer(nfg_, values_, params).optimize();
<<<<<<< HEAD
    }else if (solver_type_ == 3) {
      // TODO: something (SE-SYNC?)
    }
    // save result 
    if (save_g2o_) {
      gtsam::writeG2o(nfg_, values_, g2o_file_path_);
=======
    } else {
      log<WARNING>("Unsupported Solver");
      exit (EXIT_FAILURE);
>>>>>>> master
    }
  }
}

void GenericSolver::removeFactorsNoUpdate(
    gtsam::FactorIndices factorsToRemove) {
  // remove factors
  for (size_t index : factorsToRemove) {
    nfg_[index].reset();
  }
}
<<<<<<< HEAD
=======

}
>>>>>>> master
