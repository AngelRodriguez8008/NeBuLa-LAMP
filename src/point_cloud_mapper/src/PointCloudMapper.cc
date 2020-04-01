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
 * Authors: Erik Nelson            ( eanelson@eecs.berkeley.edu )
 */

#include <parameter_utils/ParameterUtils.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/octree/octree_iterator.h>
#include <point_cloud_mapper/PointCloudMapper.h>
#include <std_msgs/String.h>

namespace pu = parameter_utils;

PointCloudMapper::PointCloudMapper()
  : initialized_(false), map_updated_(false), incremental_unsubscribed_(false) {
  // Initialize map data container.
  map_data_.reset(new PointCloud);
}

PointCloudMapper::~PointCloudMapper() {
  if (publish_thread_.joinable()) {
    publish_thread_.join();
  }
}

bool PointCloudMapper::Initialize(const ros::NodeHandle& n) {
  name_ = ros::names::append(n.getNamespace(), "PointCloudMapper");

  if (!LoadParameters(n)) {
    ROS_ERROR("%s: Failed to load parameters.", name_.c_str());
    return false;
  }

  if (!RegisterCallbacks(n)) {
    ROS_ERROR("%s: Failed to register callbacks.", name_.c_str());
    return false;
  }

  return true;
}

bool PointCloudMapper::LoadParameters(const ros::NodeHandle& n) {
  // Load fixed frame.
  if (!pu::Get("frame_id/fixed", fixed_frame_id_))
    return false;
  map_data_->header.frame_id = fixed_frame_id_;

  // Load map parameters.
  if (!pu::Get("map/octree_resolution", octree_resolution_))
    return false;
  if (!pu::Get("map/b_publish_only_with_subscribers", b_publish_only_with_subscribers_))
    return false;

  // Initialize the map octree.
  map_octree_.reset(new Octree(octree_resolution_));
  map_octree_->setInputCloud(map_data_);

  initialized_ = true;

  return true;
}

bool PointCloudMapper::RegisterCallbacks(const ros::NodeHandle& n) {
  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  map_pub_ = nl.advertise<PointCloud>("octree_map", 10, true);
  incremental_map_pub_ =
      nl.advertise<PointCloud>("octree_map_updates", 10, true);
  map_frozen_pub_ = nl.advertise<PointCloud>("octree_map_frozen", 10, false);
  map_info_pub_ = nl.advertise<core_msgs::MapInfo>("map_info", 10, false);

  return true;
}

void PointCloudMapper::Reset() {
  map_data_.reset(new PointCloud);
  map_data_->header.frame_id = fixed_frame_id_;
  map_octree_.reset(new Octree(octree_resolution_));
  map_octree_->setInputCloud(map_data_);

  initialized_ = true;
}

bool PointCloudMapper::InsertPoints(const PointCloud::ConstPtr& points,
                                    PointCloud* incremental_points) {
  if (!initialized_) {
    ROS_ERROR("%s: Not initialized.", name_.c_str());
    return false;
  }

  if (incremental_points == NULL) {
    ROS_ERROR("%s: Incremental point cloud argument is null.", name_.c_str());
    return false;
  }
  incremental_points->clear();

  // Try to get the map mutex from the publisher. If the publisher is using it,
  // we will just not insert this point cloud right now. It'll be added when the
  // map is regenerated by loop closure.
  if (map_mutex_.try_lock()) {
    double min_x, min_y, min_z, max_x, max_y, max_z;

    bool isInBox;
    // Iterate over points in the input point cloud, inserting them into the map
    // if there is not already a point in the same voxel.
    for (size_t ii = 0; ii < points->points.size(); ++ii) {
      const pcl::PointXYZI p = points->points[ii];
      map_octree_->getBoundingBox(min_x, min_y, min_z, max_x, max_y, max_z);
      isInBox = (p.x >= min_x && p.x <= max_x) &&
          (p.y >= min_y && p.y <= max_y) && (p.z >= min_z && p.z <= max_z);
      if (!isInBox || !map_octree_->isVoxelOccupiedAtPoint(p)) {
        // if (!map_octree_->isVoxelOccupiedAtPoint(p)) {
        map_octree_->addPointToCloud(p, map_data_);
        incremental_points->push_back(p);
        // if (p.x < min_x){min_x = p.x;}
        // if (p.y < min_y){min_y = p.y;}
        // if (p.z < min_z){min_z = p.z;}
        // if (p.x > max_x){max_x = p.x;}
        // if (p.y > max_y){max_y = p.y;}
        // if (p.z > max_z){max_z = p.z;}
      }
    }
    map_mutex_.unlock();
  } else {
    // This won't happen often.
    ROS_WARN(
        "%s: Failed to update map: map publisher has a hold of the thread. "
        "Turn off any subscriptions to the 3D map topic to prevent this from "
        "happening.",
        name_.c_str());
  }

  // Publish the incremental map update.
  incremental_points->header = points->header;
  incremental_points->header.frame_id = fixed_frame_id_;
  PublishMapUpdate(*incremental_points);

  map_updated_ = true;
  return true;
}

bool PointCloudMapper::ApproxNearestNeighbors(const PointCloud& points,
                                              PointCloud* neighbors) {
  if (!initialized_) {
    ROS_ERROR("%s: Not initialized.", name_.c_str());
  }

  if (neighbors == NULL) {
    ROS_ERROR("%s: Output argument is null.", name_.c_str());
  }

  neighbors->points.clear();

  // Iterate over points in the input point cloud, finding the nearest neighbor
  // for every point and storing it in the output array.
  for (size_t ii = 0; ii < points.points.size(); ++ii) {
    // Search for nearest neighbor and store.
    float unused = 0.f;
    int result_index = -1;

    map_octree_->approxNearestSearch(points.points[ii], result_index, unused);
    if (result_index >= 0)
      neighbors->push_back(map_data_->points[result_index]);
  }

  return neighbors->points.size() > 0;
}

void PointCloudMapper::PublishMap() {

  if (map_pub_.getNumSubscribers() > 0 || !b_publish_only_with_subscribers_){
    if (initialized_ && map_updated_ ) {
      // Use a new thread to publish the map to avoid blocking main thread
      // on concurrent calls.
      if (publish_thread_.joinable()) {
        publish_thread_.join();
      }
      publish_thread_ = std::thread(&PointCloudMapper::PublishMapThread, this);
    }
  }
}

void PointCloudMapper::PublishMapThread() {
  map_mutex_.lock();
  map_pub_.publish(map_data_);

  // Don't publish again until we get another map update.
  map_updated_ = false;
  map_mutex_.unlock();
}

void PointCloudMapper::PublishMapFrozen() {
  ROS_INFO_STREAM("1");
  if (initialized_ && map_frozen_pub_.getNumSubscribers() > 0) {
    ROS_INFO_STREAM("2");
    // Use a new thread to publish the map to avoid blocking main thread
    // on concurrent calls.
    if (publish_frozen_thread_.joinable()) {
      ROS_INFO_STREAM("3");
      publish_frozen_thread_.join();
    }
    publish_frozen_thread_ = std::thread(&PointCloudMapper::PublishMapFrozenThread, this);
  }
}

void PointCloudMapper::PublishMapFrozenThread() {
  map_frozen_mutex_.lock();
  ROS_INFO_STREAM("Publishing frozen map");
  map_frozen_pub_.publish(map_data_);

  // Don't publish again until we get another map update.
  map_frozen_mutex_.unlock();
}

void PointCloudMapper::PublishMapUpdate(const PointCloud& incremental_points) {
  // Publish the incremental points for visualization.
  incremental_map_pub_.publish(incremental_points);
}

void PointCloudMapper::PublishMapInfo(){
  // When do we want to publish: When points are inserted or the one done in Base station. Why is it so in base station
  // 
  core_msgs::MapInfo map_info;

  // If the map has been recently updated
  if (initialized_ && map_updated_) {
    // Collect map properties
    map_info.header.stamp = ros::Time (map_data_->header.stamp/((uint64_t) 1e6), (map_data_->header.stamp%((uint64_t) 1e6))*1e3);
    map_info.header.frame_id = map_data_->header.frame_id;
    map_info.size = map_data_->size();
    map_info.initialized = initialized_;

    // Get iterator to the octree
    // pcl::octree::OctreeBreadthFirstIterator<pcl::octree::OctreePointCloud> octree_itr = map_octree_.breadth_begin();

    

    // Start stepping through 
    int current_depth = -1;
    int depth = -1;
    int target_depth;
    int depth_count = 0;
    std::vector<int> count_per_depth;
    double voxel_side_at_depth; 

    int tree_depth = map_octree_->getTreeDepth();

    // find the depth that we want - depth first search
    auto df_itr = map_octree_->depth_begin();
    while (df_itr != map_octree_->depth_end()){
      depth = df_itr.getCurrentOctreeDepth();
      voxel_side_at_depth = std::sqrt(map_octree_->getVoxelSquaredSideLen(depth));

      if (voxel_side_at_depth > 0.4 && voxel_side_at_depth < 0.7){
        // If the side length is around 0.5
        target_depth = depth;
        break;
      }
      df_itr++;
    }

    // Reset iterator
    auto octree_itr = map_octree_->breadth_begin();

    while (octree_itr != map_octree_->breadth_end()){
      // Check the current depth 
      depth = octree_itr.getCurrentOctreeDepth();
      if (depth < target_depth){
        // Skip this - doesn't contain what we want
        octree_itr++;
        continue;
      } else if (depth > target_depth){
        break;
      }
      // Else is a leaf node

      depth_count++;
      octree_itr++;
    }

    // Compute volume
    double volume = depth_count*std::pow(voxel_side_at_depth,3.0);

    ROS_INFO_STREAM("Point cloud Volume is: " << volume << ", from " << depth_count << " voxels with side length " << voxel_side_at_depth);
    map_info.volume = volume;
    
    // Publish
    map_info_pub_.publish(map_info);
  }
}    


    //   if (depth != current_depth){
    //     if (current_depth > -1){
    //       ROS_INFO_STREAM("Count from depth " << current_depth << " is " << depth_count);
    //       count_per_depth.push_back(depth_count);
    //     }
    //     current_depth = depth;
    //     voxel_side_at_depth = std::sqrt(map_octree_->getVoxelSquaredSideLen(depth));
    //     ROS_INFO_STREAM("Depth is " << depth << ". Side length at depth is " << voxel_side_at_depth);
    //     // At a new depth 
    //     depth_count = 1;
    //   } else {
    //     depth_count++;
    //   }
    //   octree_itr++;
    // }
    // Need to check with this octree_resolution_