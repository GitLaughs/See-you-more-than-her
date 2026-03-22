/**
 * @file debug_data_interface.cpp
 * @brief TCP JSON debug data interface for Aurora desktop tool
 *
 * Provides real-time streaming of lidar point cloud, detection results,
 * and obstacle avoidance data to the Aurora debugging framework via
 * newline-delimited JSON over TCP.
 */

#include "../include/debug_data_interface.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ssne_demo {

bool DebugDataInterface::Start(int port) {
  port_ = port;
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::fprintf(stderr, "[DEBUG] Failed to create socket\n");
    return false;
  }

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    std::fprintf(stderr, "[DEBUG] Bind failed on port %d\n", port);
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  if (listen(server_fd_, 1) < 0) {
    std::fprintf(stderr, "[DEBUG] Listen failed\n");
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Set server socket to non-blocking for accept
  int flags = fcntl(server_fd_, F_GETFL, 0);
  fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

  std::printf("[DEBUG] TCP server listening on port %d\n", port);
  return true;
}

void DebugDataInterface::Stop() {
  if (client_fd_ >= 0) {
    close(client_fd_);
    client_fd_ = -1;
  }
  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }
}

void DebugDataInterface::SendJson(const std::string& json) {
  // Try to accept a new client if none connected
  if (client_fd_ < 0 && server_fd_ >= 0) {
    struct sockaddr_in client_addr {};
    socklen_t len = sizeof(client_addr);
    int fd =
        accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (fd >= 0) {
      client_fd_ = fd;
      std::printf("[DEBUG] Client connected from %s:%d\n",
                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
  }

  if (client_fd_ < 0) return;

  std::string msg = json + "\n";
  ssize_t sent = write(client_fd_, msg.c_str(), msg.size());
  if (sent < 0) {
    std::fprintf(stderr, "[DEBUG] Send failed, closing client\n");
    close(client_fd_);
    client_fd_ = -1;
  }
}

std::string DebugDataInterface::SerializePointCloud(
    const std::vector<LidarSample>& samples) {
  std::ostringstream ss;
  ss << "[";
  for (size_t i = 0; i < samples.size(); ++i) {
    if (i > 0) ss << ",";
    float angle_deg = samples[i].angle_deg * 180.0f / 3.14159265f;
    ss << "{\"a\":" << angle_deg << ",\"d\":" << samples[i].distance_m
       << ",\"q\":" << samples[i].quality << "}";
  }
  ss << "]";
  return ss.str();
}

std::string DebugDataInterface::SerializeDetections(
    const DetectionResult& result, const Yolov8Detector& detector) {
  std::ostringstream ss;
  ss << "[";
  for (size_t i = 0; i < result.detections.size(); ++i) {
    if (i > 0) ss << ",";
    const auto& d = result.detections[i];
    ss << "{\"class\":\"" << detector.ClassName(d.class_id) << "\""
       << ",\"score\":" << d.score << ",\"box\":[" << d.box[0] << ","
       << d.box[1] << "," << d.box[2] << "," << d.box[3] << "]}";
  }
  ss << "]";
  return ss.str();
}

std::string DebugDataInterface::SerializeObstacleZones(
    const std::vector<ObstacleZone>& zones) {
  std::ostringstream ss;
  ss << "[";
  for (size_t i = 0; i < zones.size(); ++i) {
    if (i > 0) ss << ",";
    ss << "{\"angle_start\":" << zones[i].angle_start_deg
       << ",\"angle_end\":" << zones[i].angle_end_deg
       << ",\"min_dist\":" << zones[i].min_distance_m
       << ",\"blocked\":" << (zones[i].blocked ? "true" : "false") << "}";
  }
  ss << "]";
  return ss.str();
}

void DebugDataInterface::SendPointCloud(
    const std::vector<LidarSample>& samples) {
  std::string json =
      "{\"type\":\"pointcloud\",\"points\":" + SerializePointCloud(samples) +
      "}";
  SendJson(json);
}

void DebugDataInterface::SendDetections(const DetectionResult& result,
                                        const Yolov8Detector& detector) {
  std::string json =
      "{\"type\":\"detections\",\"data\":" +
      SerializeDetections(result, detector) + "}";
  SendJson(json);
}

void DebugDataInterface::SendObstacleZones(
    const std::vector<ObstacleZone>& zones) {
  std::string json =
      "{\"type\":\"obstacle_zones\",\"zones\":" +
      SerializeObstacleZones(zones) + "}";
  SendJson(json);
}

void DebugDataInterface::SendFrame(
    const std::vector<LidarSample>& samples, const DetectionResult& result,
    const Yolov8Detector& detector, const std::vector<ObstacleZone>& zones) {
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();

  std::ostringstream ss;
  ss << "{\"type\":\"frame\""
     << ",\"timestamp_ms\":" << ms
     << ",\"pointcloud\":" << SerializePointCloud(samples)
     << ",\"detections\":" << SerializeDetections(result, detector)
     << ",\"obstacle_zones\":" << SerializeObstacleZones(zones) << "}";
  SendJson(ss.str());
}

std::vector<ObstacleZone> ComputeObstacleZones(
    const std::vector<LidarSample>& samples, int num_sectors,
    float block_threshold_m) {
  std::vector<ObstacleZone> zones(num_sectors);
  float sector_size = 360.0f / static_cast<float>(num_sectors);

  for (int i = 0; i < num_sectors; ++i) {
    zones[i].angle_start_deg = i * sector_size;
    zones[i].angle_end_deg = (i + 1) * sector_size;
    zones[i].min_distance_m = 999.0f;
    zones[i].blocked = false;
  }

  for (const auto& s : samples) {
    float angle_deg = s.angle_deg * 180.0f / 3.14159265f;
    // Normalize to [0, 360)
    while (angle_deg < 0.0f) angle_deg += 360.0f;
    while (angle_deg >= 360.0f) angle_deg -= 360.0f;

    int sector = static_cast<int>(angle_deg / sector_size);
    if (sector >= num_sectors) sector = num_sectors - 1;

    if (s.distance_m > 0.01f && s.distance_m < zones[sector].min_distance_m) {
      zones[sector].min_distance_m = s.distance_m;
    }
  }

  for (auto& z : zones) {
    z.blocked = (z.min_distance_m < block_threshold_m);
  }

  return zones;
}

}  // namespace ssne_demo
