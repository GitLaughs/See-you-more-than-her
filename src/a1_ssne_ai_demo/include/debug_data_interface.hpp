#pragma once

#include <string>
#include <vector>
#include "lidar_sdk_adapter.hpp"
#include "yolov8_detector.hpp"

namespace ssne_demo {

/// Obstacle avoidance zone info sent to the debug tool
struct ObstacleZone {
  float angle_start_deg;   // sector start angle
  float angle_end_deg;     // sector end angle
  float min_distance_m;    // closest obstacle in this sector
  bool blocked;            // whether this sector is blocked
};

/// Debug data interface for the Aurora desktop tool.
///
/// Serializes detection results, lidar point cloud, and obstacle avoidance
/// data as JSON for TCP streaming to the Aurora debugging framework.
/// The Aurora tool can then render:
///   - 3D point cloud visualization
///   - Obstacle avoidance map
///   - Detection overlay
///
/// Protocol: newline-delimited JSON over TCP.
/// Each message is a single JSON object followed by '\n'.
class DebugDataInterface {
 public:
  /// Initialize the TCP server on the given port
  bool Start(int port);

  /// Stop the TCP server and close all connections
  void Stop();

  /// Send lidar point cloud data as a JSON message
  /// Format: {"type":"pointcloud","points":[{"a":deg,"d":m,"q":quality},...]}
  void SendPointCloud(const std::vector<LidarSample>& samples);

  /// Send YOLOv8 detection results
  /// Format: {"type":"detections","data":[{"class":"person","score":0.95,
  ///          "box":[x1,y1,x2,y2]},...]}
  void SendDetections(const DetectionResult& result,
                      const Yolov8Detector& detector);

  /// Send obstacle avoidance zone information
  /// Format: {"type":"obstacle_zones","zones":[{"angle_start":0,"angle_end":60,
  ///          "min_dist":0.35,"blocked":true},...]}
  void SendObstacleZones(const std::vector<ObstacleZone>& zones);

  /// Send combined frame data (all above in one message)
  /// Format: {"type":"frame","timestamp_ms":...,"pointcloud":[...],"detections":[...],
  ///          "obstacle_zones":[...]}
  void SendFrame(const std::vector<LidarSample>& samples,
                 const DetectionResult& result,
                 const Yolov8Detector& detector,
                 const std::vector<ObstacleZone>& zones);

  bool connected() const { return client_fd_ >= 0; }

 private:
  std::string SerializePointCloud(const std::vector<LidarSample>& samples);
  std::string SerializeDetections(const DetectionResult& result,
                                  const Yolov8Detector& detector);
  std::string SerializeObstacleZones(const std::vector<ObstacleZone>& zones);
  void SendJson(const std::string& json);

  int server_fd_{-1};
  int client_fd_{-1};
  int port_{0};
};

/// Compute obstacle avoidance zones from lidar scan data.
/// Divides 360° into `num_sectors` equal sectors and finds the minimum
/// distance in each. A sector is "blocked" if min_distance < threshold.
std::vector<ObstacleZone> ComputeObstacleZones(
    const std::vector<LidarSample>& samples,
    int num_sectors = 6,
    float block_threshold_m = 0.5f);

}  // namespace ssne_demo
