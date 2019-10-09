/**
 *  @brief Test cases for talker class
 *
 *  This file shows an example usage of gtest.
 */

#include <gtest/gtest.h>
#include <factor_handlers/OdometryHandler.h>

class OdometryHandlerTest : public ::testing::Test {

public:

  OdometryHandlerTest() {
    // Load Params
    system("rosparam load $(rospack find factor_handlers)/config/odom_parameters.yaml");
  }

  OdometryHandler oh;

protected:

    // Odometry Callbacks ------------------------------------------------
    void LidarOdometryCallback(const nav_msgs::Odometry::ConstPtr& msg) {
      oh.LidarOdometryCallback(msg);
    } 

    void VisualOdometryCallback(const nav_msgs::Odometry::ConstPtr& msg) {
      oh.LidarOdometryCallback(msg);
    }  

    void WheelOdometryCallback(const nav_msgs::Odometry::ConstPtr& msg) {
      oh.LidarOdometryCallback(msg);
    } 

    // Utilities 
    template <typename T>
    int CheckBufferSize(const std::vector<T>& x) {
      return oh.CheckBufferSize<T>(x);
    }

    template <typename T1, typename T2>
    bool InsertMsgInBuffer(typename T1::ConstPtr& msg, std::vector<T2>& buffer) {
      return oh.InsertMsgInBuffer<T1, T2>(msg, buffer);
    }

    gtsam::Pose3 GetTransform(PoseCovStampedPair pose_cov_stamped_pair) {
      return oh.GetTransform(pose_cov_stamped_pair);
    }

    gtsam::SharedNoiseModel GetCovariance(PoseCovStampedPair pose_cov_stamped_pair) {
      return oh.GetCovariance(pose_cov_stamped_pair);
    }
    
    std::pair<ros::Time, ros::Time> GetTimeStamps(PoseCovStampedPair pose_cov_stamped_pair) {
      return oh.GetTimeStamps(pose_cov_stamped_pair);
    }

    double CalculatePoseDelta(OdomPoseBuffer& odom_buffer){
      return oh.CalculatePoseDelta(odom_buffer);
    }   

    std::vector<geometry_msgs::PoseWithCovarianceStamped> lidar_odometry_buffer_ = oh.lidar_odometry_buffer_;

  private:    

};

// Test we pass ----------------------------------------------------------------------

/* TEST Initialize */ 
TEST_F(OdometryHandlerTest, Initialization) {
   ros::NodeHandle nh;
   bool result = oh.Initialize(nh);
   ASSERT_TRUE(result);
}

/* TEST CheckBufferSize */ 
TEST_F(OdometryHandlerTest, TestCheckBufferSize) {
  std::vector<PoseCovStamped> myBuffer;
  PoseCovStamped my_msg;
  myBuffer.push_back(my_msg);
  int size = CheckBufferSize(myBuffer);
  EXPECT_EQ(size, 1);
}

/* TEST CalculatePoseDelta */
TEST_F (OdometryHandlerTest, TestCalculatePoseDelta){
  // Create a buffer
  OdomPoseBuffer myBuffer; 
  // Create two messages
  geometry_msgs::PoseWithCovarianceStamped msg_first; 
  geometry_msgs::PoseWithCovarianceStamped msg_second;
  // Fill the two messages
  msg_first.pose.pose.position.x = 1; 
  msg_first.pose.pose.position.y = 0; 
  msg_first.pose.pose.position.z = 0; 
  msg_first.pose.pose.orientation.x = 0;
  msg_first.pose.pose.orientation.y = 0;
  msg_first.pose.pose.orientation.z = 0;
  msg_first.pose.pose.orientation.w = 1;
  msg_second.pose.pose.position.x = 0; 
  msg_second.pose.pose.position.y = 0; 
  msg_second.pose.pose.position.z = 0;
  msg_second.pose.pose.orientation.x = 0;
  msg_second.pose.pose.orientation.y = 0;
  msg_second.pose.pose.orientation.z = 0;
  msg_second.pose.pose.orientation.w = 1;
  // Push messages to buffer
  myBuffer.push_back(msg_first); 
  myBuffer.push_back(msg_second);   
  // Call the method to test 
  int size = CheckBufferSize(myBuffer);
  EXPECT_EQ(size, 2);
  double delta = CalculatePoseDelta(myBuffer);   
  EXPECT_EQ(delta, 1);
}

// Getters ------------------------------------------------------------

/* TEST GetTransform */
TEST_F (OdometryHandlerTest, TestGetTransform) {
  PoseCovStampedPair pose_cov_stamped_pair;
  geometry_msgs::Pose pose1;
  pose1.position.x = 0;
  pose1.position.y = 0;
  pose1.position.z = 0;
  pose1.orientation.x = 0;
  pose1.orientation.y = 0;
  pose1.orientation.z = 0;
  pose1.orientation.w = 1;
  geometry_msgs::Pose pose2;
  pose2.position.x = 1;
  pose2.position.y = 0;
  pose2.position.z = 0;
  pose2.orientation.x = 0;
  pose2.orientation.y = 0;
  pose2.orientation.z = 0;
  pose2.orientation.w = 1;
  pose_cov_stamped_pair.first.pose.pose = pose1;
  pose_cov_stamped_pair.second.pose.pose = pose2;
  gtsam::Pose3 transform_actual = GetTransform(pose_cov_stamped_pair);
  gtsam::Point3 position = gtsam::Point3(1,0,0);
  gtsam::Rot3 rotation = gtsam::Rot3(1,0,0,0,1,0,0,0,1);
  gtsam::Pose3 transform_expected = gtsam::Pose3(rotation, position);
  ASSERT_TRUE(transform_actual.equals(transform_expected));
}

TEST_F(OdometryHandlerTest, TestGetCovariance) {
  PoseCovStampedPair pose_cov_stamped_pair;
  PoseCovStamped pose_cov_stamped_1;
  PoseCovStamped pose_cov_stamped_2;
  for (size_t i = 0; i < 36; i++) {
    pose_cov_stamped_1.pose.covariance[i] = 1;
    pose_cov_stamped_2.pose.covariance[i] = 3;
  }
  pose_cov_stamped_pair.first = pose_cov_stamped_1;
  pose_cov_stamped_pair.second = pose_cov_stamped_2;
  gtsam::SharedNoiseModel noise_actual = GetCovariance(pose_cov_stamped_pair);
  gtsam::Matrix66 covariance_expected;
  for (size_t i = 0; i < 6; i++) {
    for (size_t j = 0; j < 6; j++) {
      covariance_expected(i,j) = 2;
    }
  }
  gtsam::SharedNoiseModel noise_expected =
      gtsam::noiseModel::Gaussian::Covariance(covariance_expected);
  ASSERT_TRUE((*noise_actual).equals(*noise_expected));
}

// Nobuhiro is working on GetCovariance function unit test
/* TEST  GetTimeStamps */
TEST_F(OdometryHandlerTest, TestGetTimeStamps) {
  double t1 = 1.0;
  double t2 = 2.0;
  ros::Time t1_ros;
  ros::Time t2_ros;
  t1_ros.fromSec(t1);
  t2_ros.fromSec(t2);
  PoseCovStampedPair pose_cov_stamped_pair;
  pose_cov_stamped_pair.first.header.stamp = t1_ros;
  pose_cov_stamped_pair.second.header.stamp = t2_ros;
  std::pair<ros::Time, ros::Time> time_stamp_pair_actual = 
    GetTimeStamps(pose_cov_stamped_pair);
  EXPECT_EQ(time_stamp_pair_actual.first, t1_ros);
  EXPECT_EQ(time_stamp_pair_actual.second, t2_ros);
}


// Test we pass but need more testing/implementation ---------------------------------


// Test we don't pass ----------------------------------------------------------------
/* TEST InsertMsgInBuffer */
// TEST_F(OdometryHandlerTest, InsertMsgInBuffer) {
//    // Create a buffer
//     std::vector<PoseCovStamped> myBuffer;
//     // Create a message
//     Odometry::ConstPtr msg;
//     // Call the method
//     bool result = InsertMsgInBuffer<Odometry, PoseCovStamped>(msg, myBuffer);
//    // Check result is correct
//    ASSERT_TRUE(result);
// }



// Initialize: Done

// LoadParameters

// RegisterCallbacks

// LidarOdometryCallback

// VisualOdometryCallback

// WheelOdometryCallback

// PointCloudCallback

// CheckOdometryBuffer

// CalculatePoseDelta: Done (Nobuhiro)

// PrepareFactor: (Nobuhiro)

// MakeFactor: (Nobuhiro)

// GetTransform: Done (Nobuhiro)

// GetCovariance: Done (Nobuhiro)

// GetTimeStamps: Done (Nobuhiro)

// GetKeyedScanAtTime:

// ToGtsam: (Kamak)

// GetDeltaBetweenTimes: (Kamak)

// GetDeltaBetweenPoses: (Kamak)

// GetPoseAtTime: (Kamak)

// GetPosesAtTimes: (Kamak)

// InsertMsgInBuffer: Done

// CheckBufferSize: Done (Kamak)


int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  ros::init(argc, argv, "test_odometry_handler");
  return RUN_ALL_TESTS();
}