#ifndef APRIL_TAG_HANDLER_H_
#define APRIL_TAG_HANDLER_H_
// Includes
#include "factor_handlers/ArtifactHandler.h"
#include <core_msgs/AprilTag.h>

class AprilTagHandler : public ArtifactHandler {
    public:
    // Constructor
    AprilTagHandler();
    // Destructor
    virtual ~AprilTagHandler() = default;

    /*! \brief Initialize parameters and callbacks. 
     * n - Nodehandle
     * Returns bool
     */
    virtual bool Initialize(const ros::NodeHandle& n);

    /*! \brief  Get ground truth data from April tag node key.  
     * Returns  Ground truth information
     */
    gtsam::Pose3 GetGroundTruthData(const gtsam::Symbol april_tag_key);
    
    /*! \brief Gives the factors to be added and clears to start afresh.
     * Returns New factor data
     */
    virtual std::shared_ptr<FactorData> GetData();

    protected:

    /*! \brief Load April Tag parameters. 
     * n - Nodehandle
     * Returns bool
     */
    virtual bool LoadParameters(const ros::NodeHandle& n);

    /*! \brief Register Online callbacks. 
     * n - Nodehandle
     * Returns bool
     */
    virtual bool RegisterOnlineCallbacks(const ros::NodeHandle& n); 

    /*! \brief  Callback for April Tag.
     * Returns  Void
     */
    void AprilTagCallback(const core_msgs::AprilTag& msg);

    /*! \brief  Convert April tag message to Artifact message.
     * Returns  Artifacts message
     */
    core_msgs::Artifact ConvertAprilTagMsgToArtifactMsg(const core_msgs::AprilTag& msg) const;
    
    /*! \brief  Add artifact data
     * Returns  Void
     */
    virtual void AddArtifactData(const gtsam::Symbol artifact_key, const ros::Time time_stamp, const gtsam::Point3 transform, const gtsam::SharedNoiseModel noise);

    private:
    // April related parameters
    // GT AprilTag world coordinates
    double calibration_left_x_;
    double calibration_left_y_;
    double calibration_left_z_;

    double calibration_right_x_;
    double calibration_right_y_;
    double calibration_right_z_;
    
    double distal_x_;
    double distal_y_;
    double distal_z_;
    
    // April Tag output data
    AprilTagData artifact_data_;

    // Test class
    friend class TestAprilTagHandler;
};

#endif  // APRIL_TAG_HANDLER_H_