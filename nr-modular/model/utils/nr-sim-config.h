/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_SIM_CONFIG_H
#define NR_SIM_CONFIG_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/vector.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <map>

namespace ns3
{

/**
 * @brief Per-UE waypoint specification
 */
struct UeWaypointConfig
{
    std::vector<Vector> waypoints;  // List of waypoint positions
    double speed = 3.0;             // Speed in m/s for this UE
};

/**
 * @brief Configuration structure for NrSimulationManager
 * 
 * Loaded from JSON file. Supports both traditional parameters and
 * per-UE waypoint mobility specifications.
 */
class NrSimConfig : public Object
{
  public:
    static TypeId GetTypeId();
    
    NrSimConfig();
    ~NrSimConfig() override;

    /**
     * @brief Load configuration from JSON file
     * @param filename Path to JSON config file
     * @return true if successful, false otherwise
     */
    bool LoadFromJson(const std::string& filename);

    /**
     * @brief Validate configuration
     * @return true if valid, false otherwise
     */
    bool Validate() const;

    /**
     * @brief Print configuration to output stream
     * @param os Output stream
     */
    void Print(std::ostream& os) const;

    /**
     * @brief Check if a specific UE has custom waypoints
     * @param ueId UE identifier
     * @return true if waypoints exist for this UE
     */
    bool HasUeWaypoints(uint32_t ueId) const;

    /**
     * @brief Get waypoint configuration for a specific UE
     * @param ueId UE identifier
     * @return UeWaypointConfig structure
     */
    UeWaypointConfig GetUeWaypoints(uint32_t ueId) const;

    // ========================================================================
    // CONFIGURATION PARAMETERS
    // ========================================================================

    // Topology parameters
    struct TopologyParams
    {
        uint32_t gnbCount = 1;
        uint32_t ueCount = 10;
        bool useFilePositions = false;
        std::string positionFile = "input/node_positions.txt";
        double areaSize = 1000.0;

        // Flexible deployment options
        bool strictFileMode = true;
        std::string uePlacementStrategy = "uniform";

        // Hotspot placement
        uint32_t numHotspots = 3;
        double hotspotRadius = 100.0;

        // Clustered placement
        uint32_t numClusters = 5;
        double clusterRadius = 50.0;
        double uesPerCluster = 10;

        // Grid placement
        double gridSpacing = 100.0;

        // Position vectors (if loaded from file)
        std::vector<Vector> gnbPositions;  // gNB positions from GUI
        std::vector<Vector> uePositions;   // UE positions from GUI
    } topology;

    // Channel parameters
    struct ChannelParams
    {
        std::string propagationModel = "UMa";
        double frequency = 3.5e9;  // Hz
        double bandwidth = 20e6;   // Hz
    } channel;

    // Mobility parameters
    struct MobilityParams
    {
        std::string defaultModel = "RandomWalk";  // Fallback mobility model
        double defaultSpeed = 3.0;                 // Default speed in m/s
        
        // Per-UE waypoints (loaded from JSON)
        // Key: UE ID, Value: waypoint configuration
        std::map<uint32_t, UeWaypointConfig> ueWaypoints;
    } mobility;

    // Network parameters
    // struct NetworkParams
    // {
    //     double pgwToRemoteHostDelay = 50.0; // ms
    //     double pgwToRemoteHostBw = 100.0;   // Mbps
    // } network;

    bool enableConnectivityTest;
    bool enableTrafficFlowTest;

    // Traffic parameters
    struct TrafficParams
    {
        double udpRateDl = 10.0;       // Mbps
        uint32_t packetSizeDl = 1024;  // bytes
        double udpRateUl = 5.0;        // Mbps
        uint32_t packetSizeUl = 512;   // bytes
        bool enableDownlink = true;
        bool enableUplink = true;
        bool enableFlowMonitoring = true;
        double startTime = 0.0;        // seconds
        double duration = 10.0;        // seconds
        
    } traffic;

    // Simulation parameters
    double simDuration = 10.0;  // seconds
    bool logTraffic = false;    // Enable detailed traffic logging

    // Monitoring parameters
    struct MonitoringParams
    {
        double monitorInterval = 1.0; // seconds
    } monitoring;

    // Debug parameters
    struct DebugParams
    {
        bool enableDebugLogs = false;  // Enable debug logging
        bool enableVerboseHandoverLogs = false; // Verbose handover logs
    } debug;


    // Metrics parameters
    bool enableFlowMonitor = true;
    std::string outputFilePath = "output/results.txt";

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Parse topology section from JSON
     * @param j JSON object
     */
    void ParseTopology(const nlohmann::json& j);

    /**
     * @brief Parse channel section from JSON
     * @param j JSON object
     */
    void ParseChannel(const nlohmann::json& j);

    /**
     * @brief Parse mobility section from JSON
     * @param j JSON object
     */
    void ParseMobility(const nlohmann::json& j);

    /**
     * @brief Parse traffic section from JSON
     * @param j JSON object
     */
    void ParseTraffic(const nlohmann::json& j);

    /**
     * @brief Parse simulation section from JSON
     * @param j JSON object
     */
    void ParseSimulation(const nlohmann::json& j);

    /**
     * @brief Parse monitoring section from JSON
     * @param j JSON object
     */
    void ParseMonitoring(const nlohmann::json& j);
    /**
     * @brief Parse debug section from JSON
     * @param j JSON object
     */
    void ParseDebug(const nlohmann::json& j);

    /**
     * @brief Parse metrics section from JSON
     * @param j JSON object
     */
    void ParseMetrics(const nlohmann::json& j);

    /**
     * @brief Parse per-UE waypoints from JSON
     * @param j JSON object containing ueWaypoints
     */
    void ParseUeWaypoints(const nlohmann::json& j);
};

} // namespace ns3

#endif // NR_SIM_CONFIG_H