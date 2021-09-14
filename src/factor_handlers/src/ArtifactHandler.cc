// Includes
#include "factor_handlers/ArtifactHandler.h"

// Constructor
ArtifactHandler::ArtifactHandler()
  : largest_artifact_id_(0),
    use_artifact_loop_closure_(false),
    is_pgo_initialized(false) {}

/*! \brief Initialize parameters and callbacks.
 * n - Nodehandle
 * Returns bool
 */
bool ArtifactHandler::Initialize(const ros::NodeHandle& n) {
  name_ = ros::names::append(n.getNamespace(), "Artifact");

  if (!LoadParameters(n)) {
    ROS_ERROR("%s: Failed to load artifact parameters.", name_.c_str());
    return false;
  }
  // Need to change this
  if (!RegisterCallbacks(n, false)) {
    ROS_ERROR("%s: Failed to register artifact callback.", name_.c_str());
    return false;
  }

  last_existing_artifacts_update_time_ = ros::Time::now();

  return true;
}

/*! \brief Load artifact parameters.
 * n - Nodehandle
 * Returns bool
 */
bool ArtifactHandler::LoadParameters(const ros::NodeHandle& n) {
  if (!pu::Get("b_artifacts_in_global", b_artifacts_in_global_))
    return false;
  if (!pu::Get("use_artifact_loop_closure", use_artifact_loop_closure_))
    return false;

  // Get the artifact prefix from launchfile to set initial unique artifact ID
  bool b_initialized_artifact_prefix_from_launchfile = true;
  std::string artifact_prefix;
  unsigned char artifact_prefix_converter[1];
  if (!pu::Get("artifact_prefix", artifact_prefix)) {
    b_initialized_artifact_prefix_from_launchfile = false;
    ROS_ERROR("Could not find node ID assosiated with robot_namespace [Artifact Handler]");
  }

  if (b_initialized_artifact_prefix_from_launchfile) {
    std::copy(artifact_prefix.begin(),
              artifact_prefix.end(),
              artifact_prefix_converter);
    artifact_prefix_ = artifact_prefix_converter[0];
  }
  return true;
}

/*! \brief Register callbacks.
 * n - Nodehandle
 * Returns bool
 */
bool ArtifactHandler::RegisterCallbacks(const ros::NodeHandle& n,
                                        bool from_log) {
  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  if (from_log)
    return RegisterLogCallbacks(n);
  else
    return RegisterOnlineCallbacks(n);
}

/*! \brief Compute transform from Artifact message.
 * Returns Transform
 */
Eigen::Vector3d
ArtifactHandler::ComputeTransform(const artifact_msgs::Artifact& msg) const {
  // Get artifact position
  Eigen::Vector3d artifact_position;
  artifact_position << msg.point.point.x, msg.point.point.y, msg.point.point.z;

  std::cout << "Artifact position in robot frame is: " << artifact_position[0]
            << ", " << artifact_position[1] << ", " << artifact_position[2]
            << std::endl;
  return artifact_position;
}

/*! \brief  Get artifacts ID from artifact key
 * Returns Artifacts ID
 */
std::string ArtifactHandler::GetArtifactID(const gtsam::Symbol artifact_key) {
  std::string artifact_id;
  for (auto it = artifact_id2key_hash.begin(); it != artifact_id2key_hash.end();
       ++it) {
    if (it->second == artifact_key) {
      artifact_id = it->first;
      return artifact_id;
    }
  }
  std::cout << "Artifact ID not found for key"
            << gtsam::DefaultKeyFormatter(artifact_key) << std::endl;
  return "";
}

/*! \brief  Callback for Artifacts.
 * Returns  Void
 */
void ArtifactHandler::ArtifactCallback(const artifact_msgs::Artifact& msg) {
  // Subscribe to artifact messages, include in pose graph, publish global
  // position Artifact information
  PrintArtifactInputMessage(msg);

  // Process artifact only is pose graph is initialized
  if (!is_pgo_initialized) {
    ROS_DEBUG("Rejecting Artifacts as pose graph not initialized.");
    return;
  }

  // Check for NaNs and reject
  if (std::isnan(msg.point.point.x) || std::isnan(msg.point.point.y) ||
      std::isnan(msg.point.point.z) || (msg.point.header.stamp.toNSec() == 0.0)) {
    ROS_WARN("Ill-formed artifact message. Rejecting Artifact message.");
    return;
  }

  // Get the transformation
  Eigen::Vector3d R_artifact_position = ComputeTransform(msg);

  // Get the artifact id
  std::string artifact_id = msg.id;

  // Artifact key
  gtsam::Symbol cur_artifact_key;
  bool b_is_new_artifact = false;

  // get artifact id / key -----------------------------------------------
  // Check if the ID of the object already exists in the object hash
  if (artifact_id2key_hash.find(artifact_id) != artifact_id2key_hash.end()) {
    cur_artifact_key = artifact_id2key_hash[artifact_id];
    ROS_DEBUG_STREAM(
        "\nArtifact Handler: artifact previously observed, artifact id "
        << artifact_id);
    std::cout << "artifact previously observed, artifact id " << artifact_id
              << " with key in pose graph "
              << gtsam::DefaultKeyFormatter(cur_artifact_key) << std::endl;

    // Fill ArtifactInfo hash
    StoreArtifactInfo(cur_artifact_key, msg);

    // do not add the artifact to artifact_data_ yet
    ROS_INFO_STREAM("Skipping adding artifact directly: " << artifact_id);
    return;
  } else {
    // New artifact - increment the id counters
    b_is_new_artifact = true;
    std::cout << "The number key is " << largest_artifact_id_
              << " with character "
              << artifact_prefix_ << std::endl;
    cur_artifact_key = gtsam::Symbol(artifact_prefix_, largest_artifact_id_);
    ++largest_artifact_id_;
    ROS_INFO_STREAM("\nArtifact Handler: new artifact observed, artifact id "
                    << artifact_id << "with key"
                    << gtsam::DefaultKeyFormatter(cur_artifact_key));
    // update hash
    artifact_id2key_hash[artifact_id] = cur_artifact_key;

    // Add key to new_keys_
    new_keys_.push_back(cur_artifact_key);

    // Fill ArtifactInfo hash
    StoreArtifactInfo(cur_artifact_key, msg);
  }

  // Generate gtsam pose
  const gtsam::Pose3 relative_pose = gtsam::Pose3(gtsam::Rot3(),
                   gtsam::Point3(R_artifact_position[0],
                                 R_artifact_position[1],
                                 R_artifact_position[2]));

  // Extract covariance
  gtsam::SharedNoiseModel noise = ExtractCovariance(msg.covariance);

  // Fill artifact_data_
  AddArtifactData(cur_artifact_key, msg.point.header.stamp, relative_pose.translation(), noise);
}

/*! \brief  Gives the factors to be added and clears to start afresh.
 * Returns  Factors
 * TODO: IN case of AprilTag this would spit out ArtifactData which is wrong
 */
std::shared_ptr<FactorData> ArtifactHandler::GetData() {
  // Add updated factor of existing artifact
  AddUpdatedArtifactData();

  // Create a temporary copy to return
  std::shared_ptr<ArtifactData> temp_artifact_data_ = std::make_shared<ArtifactData>(artifact_data_);


  // Clear artifact data
  ClearArtifactData();

  // Return artifact data
  return temp_artifact_data_;
}

/*! \brief  Create the publishers to log data.
 * Returns  Values
 */
bool ArtifactHandler::RegisterLogCallbacks(const ros::NodeHandle& n) {
  ROS_DEBUG("%s: Registering log callbacks.", name_.c_str());
  return CreatePublishers(n);
}

bool ArtifactHandler::CreatePublishers(const ros::NodeHandle& n) {
  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  // Create publisher for artifact
  artifact_pub_ = nl.advertise<artifact_msgs::Artifact>("artifact", 10);

  return true;
}

/*! \brief Register Online callbacks.
 * n - Nodehandle
 * Returns bool
 */
bool ArtifactHandler::RegisterOnlineCallbacks(const ros::NodeHandle& n) {
  ROS_DEBUG("%s: Registering online callbacks for Artifacts.", name_.c_str());

  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  artifact_sub_ = nl.subscribe(
      "artifact_relative", 10, &ArtifactHandler::ArtifactCallback, this);

  return CreatePublishers(n);
}

gtsam::Symbol ArtifactHandler::GetKeyFromID(std::string id) {
  if (artifact_id2key_hash.find(id) != artifact_id2key_hash.end()) {
    ROS_ERROR_STREAM("Artifact ID does not exist in Artifact Handler");
    return utils::GTSAM_ERROR_SYMBOL;
  }

  return artifact_id2key_hash[id];
}


/*! \brief  Updates the global pose of an artifact
 * Returns  Void
 */
bool ArtifactHandler::UpdateGlobalPosition(const gtsam::Symbol artifact_key,
                                           const gtsam::Point3 global_position) {
  if (artifact_key2info_hash_.find(artifact_key) !=
      artifact_key2info_hash_.end()) {
    artifact_key2info_hash_[artifact_key].global_position = global_position;
    return true;
  } else {
    std::cout << "Key not found in the Artifact id to key map.";
    return false;
  }
}

/*! \brief  Publish Artifact. Need to see if publish_all is still relevant
 * I am considering publishing if we have the key and the pose without
 * any further processing.
 * TODO Resolve the frame and transform between world and map
 * in output message.
 * Returns  Void
 */
void ArtifactHandler::PublishArtifacts(const gtsam::Symbol artifact_key,
                                       const gtsam::Pose3 global_pose) {
  // Get the artifact pose
  Eigen::Vector3d artifact_position = global_pose.translation();
  std::string artifact_label;

  if (!(artifact_key.chr() == 'A' || artifact_key.chr() == 'B' ||
        artifact_key.chr() == 'C' || artifact_key.chr() == 'D' ||
        artifact_key.chr() == 'E' || artifact_key.chr() == 'F' ||
        artifact_key.chr() == 'G' || artifact_key.chr() == 'H' ||
        artifact_key.chr() == 'I' || artifact_key.chr() == 'J' ||
        artifact_key.chr() == 'K' || artifact_key.chr() == 'L' ||
        artifact_key.chr() == 'M' || artifact_key.chr() == 'X')) {
    ROS_WARN("ERROR - have a non-landmark ID");
    ROS_WARN_STREAM("Bad ID is " << gtsam::DefaultKeyFormatter(artifact_key));
    return;
  }

  // Using the artifact key to publish that artifact
  ROS_DEBUG("Publishing the new artifact");
  ROS_DEBUG_STREAM("Artifact key to publish is "
                  << gtsam::DefaultKeyFormatter(artifact_key));

  // Check that the key exists
  if (artifact_key2info_hash_.count(gtsam::Key(artifact_key)) == 0) {
    ROS_WARN("Artifact key is not in hash, nothing to publish");
    return;
  }

  // Get label
  artifact_label = artifact_key2info_hash_[gtsam::Key(artifact_key)].msg.label;

  // Increment update count
  // TODO: I am moving this in Artifact callback representing the
  // number of measurements I receive of one particular
  // artifact. Need to understand the use of this.
  // artifact_key2info_hash_[artifact_key].num_updates++;

  // std::cout << "Number of updates of artifact is: "
  //           << artifact_key2info_hash_[artifact_key].num_updates
  //           << std::endl;

  // Fill artifact message
  artifact_msgs::Artifact new_msg =
      artifact_key2info_hash_[gtsam::Key(artifact_key)].msg;

  // Update the time
  new_msg.header.stamp = ros::Time::now();

  // Fill the new message positions
  new_msg.point.point.x = artifact_position[0];
  new_msg.point.point.y = artifact_position[1];
  new_msg.point.point.z = artifact_position[2];
  // TODO Need to check the frame id and transform
  // new_msg.point.header.frame_id = fixed_frame_id_;
  // Transform to world frame from map frame
  // new_msg.point = tf_buffer_.transform(
  // new_msg.point, "world", new_msg.point.header.stamp, "world");

  // Print out
  // Transform at time of message
  PrintArtifactInputMessage(new_msg);

  // Publish
  artifact_pub_.publish(new_msg);
}

/*! \brief  Print Artifact input message for debugging
 * Returns  Void
 */
void ArtifactHandler::PrintArtifactInputMessage(
    const artifact_msgs::Artifact& artifact) const {
  std::cout << "Artifact position in world is: " << artifact.point.point.x
            << ", " << artifact.point.point.y << ", " << artifact.point.point.z
            << std::endl;
  std::cout << "Frame ID is: " << artifact.point.header.frame_id << std::endl;

  std::cout << "\t Parent id: " << artifact.parent_id << std::endl;
  std::cout << "\t Confidence: " << artifact.confidence << std::endl;
  std::cout << "\t Position:\n[" << artifact.point.point.x << ", "
            << artifact.point.point.y << ", " << artifact.point.point.z << "]"
            << std::endl;
  std::cout << "\t Label: " << artifact.label << std::endl;
}

/*! \brief  Extracts covariance from artifact message and converts to
 * gtsam::SharedNoiseModel Returns  gtsam::SharedNoiseModel
 */
gtsam::SharedNoiseModel
ArtifactHandler::ExtractCovariance(const boost::array<float, 9> covariance) const {
  // Extract covariance information from the message
  gtsam::Matrix66 cov = gtsam::Matrix66::Zero();
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      cov(3+i, 3+j) = static_cast<double>(covariance[3 * i + j]);
    }
  cov(0, 0) = 1e10;
  cov(1, 1) = 1e10;
  cov(2, 2) = 1e10;
  // std::cout << cov.matrix();

  gtsam::SharedNoiseModel noise = gtsam::noiseModel::Gaussian::Covariance(cov);
  // noise->print();
  return noise;
}

/*! \brief  Clear artifact data
 * Returns  Void
 */
void ArtifactHandler::ClearArtifactData() {
  // Clear artifact data
  artifact_data_.b_has_data = false;
  artifact_data_.factors.clear();
}

/*! \brief  Add artifact data
 * Returns  Void
 */
void ArtifactHandler::AddArtifactData(
    const gtsam::Symbol cur_key,
    const ros::Time time_stamp,
    const gtsam::Point3 transform,
    const gtsam::SharedNoiseModel noise) {
  // Make new data true
  artifact_data_.b_has_data = true;
  // Fill type
  artifact_data_.type = "artifact";

  // Create and add the new artifact
  ArtifactFactor new_artifact;
  new_artifact.position = transform;
  new_artifact.covariance = noise;
  new_artifact.stamp = time_stamp;
  new_artifact.key = cur_key;

  artifact_data_.factors.push_back(new_artifact);
}

void ArtifactHandler::AddUpdatedArtifactData() {
  // wait until 60 seconds
  if ((ros::Time::now() - last_existing_artifacts_update_time_).toSec() <
      60.0) {
    return;
  }

  ROS_INFO_STREAM("ArtifactHandler: Add UpdatedArtifact to artifact_data_");

  // loop over the artifact_key2info_hash_,
  for (auto it = artifact_id2key_hash.begin(); it != artifact_id2key_hash.end();
       it++) {
    // Get the artifact id
    std::string artifact_id = it->first;

    // Artifact key
    gtsam::Symbol cur_artifact_key = it->second;

    if (artifact_key2info_hash_[cur_artifact_key]
            .b_included_in_artifact_data_ == true) {
      continue;
    }

    ROS_INFO_STREAM("\n\nUpdating existing artifact to AddArtifactData "
                    << artifact_id << "\n\n");

    artifact_msgs::Artifact msg = artifact_key2info_hash_[cur_artifact_key].msg;

    // Extract covariance
    gtsam::SharedNoiseModel noise = ExtractCovariance(msg.covariance);

    // Get the transformation
    Eigen::Vector3d R_artifact_position = ComputeTransform(msg);

    // Generate gtsam pose
    const gtsam::Pose3 relative_pose =
        gtsam::Pose3(gtsam::Rot3(),
                     gtsam::Point3(R_artifact_position[0],
                                   R_artifact_position[1],
                                   R_artifact_position[2]));

    // Fill artifact_data_
    AddArtifactData(cur_artifact_key,
                    msg.point.header.stamp,
                    relative_pose.translation(),
                    noise);

    artifact_key2info_hash_[cur_artifact_key].b_included_in_artifact_data_ =
        true;
  }

  last_existing_artifacts_update_time_ = ros::Time::now();
}

/*! \brief  Stores/Updated artifactInfo Hash
  * Returns  Void
  */
void ArtifactHandler::StoreArtifactInfo(const gtsam::Symbol artifact_key,
                                        const artifact_msgs::Artifact& msg) {
  // keep track of artifact info: add to hash if not added
  if (artifact_key2info_hash_.find(gtsam::Key(artifact_key)) == artifact_key2info_hash_.end()) {
    ArtifactInfo artifactinfo(msg.parent_id);
    artifactinfo.msg = msg;
    artifactinfo.num_updates = artifactinfo.num_updates+1;
    artifactinfo.b_included_in_artifact_data_ = true;
    artifact_key2info_hash_[artifact_key] = artifactinfo;
  } else {
    // Existing artifact. Hence update the artifact info
    artifact_key2info_hash_[artifact_key].num_updates += 1;
    artifact_key2info_hash_[artifact_key].msg = msg;
    artifact_key2info_hash_[artifact_key].b_included_in_artifact_data_ = false;
  }
}

/*! \brief Revert Maps and Artifact ID number upon failure in adding to pose graph.
 * Returns Void
 */
void ArtifactHandler::CleanFailedFactors(const bool success) {
  int min_key = INT_MAX;
  // If the ProcessArtifactData failed remove history
  if (!success) {
    for (auto key : new_keys_) {
      // Get the artifact id
      std::string artifact_id = artifact_key2info_hash_[key].id;

      // Remove from artifact_key2info_hash_
      artifact_key2info_hash_.erase(key);

      // Remove from artifact_id2key_hash
      artifact_id2key_hash.erase(artifact_id);

      // Find the minimum key
      if (min_key > key.index()) {
        min_key = key.index();
        largest_artifact_id_ = min_key;
      }
    }
  }
  // Clear keys in success as well as failure
  new_keys_.clear();
}
