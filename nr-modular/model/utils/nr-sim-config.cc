/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-sim-config.h"

#include "ns3/log.h"
#include "ns3/abort.h"

#include <fstream>
#include <iomanip>

// Include nlohmann/json (header-only library)
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrSimConfig");
NS_OBJECT_ENSURE_REGISTERED(NrSimConfig);

TypeId
NrSimConfig::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrSimConfig")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrSimConfig>();
    return tid;
}

NrSimConfig::NrSimConfig()
{
    NS_LOG_FUNCTION(this);
    enableConnectivityTest=false;  // Default: disabled
    enableTrafficFlowTest=false;    // Default: disabled
}

NrSimConfig::~NrSimConfig()
{
    NS_LOG_FUNCTION(this);
}

void
NrSimConfig::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

// ========================================================================
// JSON LOADING
// ========================================================================

bool
NrSimConfig::LoadFromJson(const std::string& filename)
{
    NS_LOG_FUNCTION(this << filename);

    try
    {
        // Open file
        std::ifstream file(filename);
        if (!file.is_open())
        {
            NS_LOG_ERROR("Failed to open config file: " << filename);
            return false;
        }
        // Parse JSON
        json j;
        file >> j;

        NS_LOG_INFO("Successfully parsed JSON from: " << filename);

        // Parse each section
        if (j.contains("topology"))
        {
            ParseTopology(j["topology"]);
        }

        if (j.contains("channel"))
        {
            ParseChannel(j["channel"]);
        }

        if (j.contains("mobility"))
        {
            ParseMobility(j["mobility"]);
        }

        if (j.contains("traffic"))
        {
            ParseTraffic(j["traffic"]);
        }

        if (j.contains("simulation"))
        {
            ParseSimulation(j["simulation"]);
        }
        else if (j.contains("simDuration"))
        {
            // Backward compatibility: top-level simDuration
            simDuration = j["simDuration"].get<double>();
        }

        if (j.contains("logTraffic"))
        {
            logTraffic = j["logTraffic"].get<bool>();
        }

        if (j.contains("metrics"))
        {
            ParseMetrics(j["metrics"]);
        }
        else
        {
            // Backward compatibility: top-level metrics params
            if (j.contains("enableFlowMonitor"))
            {
                enableFlowMonitor = j["enableFlowMonitor"].get<bool>();
            }
            if (j.contains("outputFilePath"))
            {
                outputFilePath = j["outputFilePath"].get<std::string>();
            }
        }

        NS_LOG_INFO("Configuration loaded successfully");
        return true;
    }
    catch (const json::exception& e)
    {
        NS_LOG_ERROR("JSON parsing error: " << e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Error loading config: " << e.what());
        return false;
    }
}

// ========================================================================
// JSON PARSING HELPERS
// ========================================================================

void
NrSimConfig::ParseTopology(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("gnbCount"))
        topology.gnbCount = j["gnbCount"].get<uint32_t>();
    if (j.contains("ueCount"))
        topology.ueCount = j["ueCount"].get<uint32_t>();
    if (j.contains("useFilePositions"))
        topology.useFilePositions = j["useFilePositions"].get<bool>();
    if (j.contains("positionFile"))
        topology.positionFile = j["positionFile"].get<std::string>();
    if (j.contains("areaSize"))
        topology.areaSize = j["areaSize"].get<double>();

    // Flexible deployment
    if (j.contains("strictFileMode"))
        topology.strictFileMode = j["strictFileMode"].get<bool>();
    if (j.contains("uePlacementStrategy"))
        topology.uePlacementStrategy = j["uePlacementStrategy"].get<std::string>();

    // Hotspot
    if (j.contains("numHotspots"))
        topology.numHotspots = j["numHotspots"].get<uint32_t>();
    if (j.contains("hotspotRadius"))
        topology.hotspotRadius = j["hotspotRadius"].get<double>();

    // Clustered
    if (j.contains("numClusters"))
        topology.numClusters = j["numClusters"].get<uint32_t>();
    if (j.contains("clusterRadius"))
        topology.clusterRadius = j["clusterRadius"].get<double>();
    if (j.contains("uesPerCluster"))
        topology.uesPerCluster = j["uesPerCluster"].get<double>();

    // Grid
    if (j.contains("gridSpacing"))
        topology.gridSpacing = j["gridSpacing"].get<double>();

    NS_LOG_INFO("Topology config parsed: " << topology.gnbCount << " gNBs, " << topology.ueCount
                                            << " UEs");
    
    // Additional position loading from file can be implemented here
    if (j.contains("gnbPositions"))
    {
        topology.gnbPositions.clear();
        const auto& gnbPosArray = j["gnbPositions"];
        
        if (gnbPosArray.is_array())
        {
            for (const auto& posObj : gnbPosArray)
            {
                if (posObj.contains("x") && posObj.contains("y") && posObj.contains("z"))
                {
                    double x = posObj["x"].get<double>();
                    double y = posObj["y"].get<double>();
                    double z = posObj["z"].get<double>();
                    topology.gnbPositions.push_back(Vector(x, y, z));
                }
            }
            NS_LOG_INFO("Loaded " << topology.gnbPositions.size() << " gNB positions from config");
        }
    }
    
    if (j.contains("uePositions"))
    {
        topology.uePositions.clear();
        const auto& uePosArray = j["uePositions"];
        
        if (uePosArray.is_array())
        {
            for (const auto& posObj : uePosArray)
            {
                if (posObj.contains("x") && posObj.contains("y") && posObj.contains("z"))
                {
                    double x = posObj["x"].get<double>();
                    double y = posObj["y"].get<double>();
                    double z = posObj["z"].get<double>();
                    topology.uePositions.push_back(Vector(x, y, z));
                }
            }
            NS_LOG_INFO("Loaded " << topology.uePositions.size() << " UE positions from config");
        }
    }

    NS_LOG_INFO("Topology config parsed: " << topology.gnbCount << " gNBs, " << topology.ueCount
                                            << " UEs"); 

}

void
NrSimConfig::ParseChannel(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("propagationModel"))
        channel.propagationModel = j["propagationModel"].get<std::string>();
    if (j.contains("frequency"))
        channel.frequency = j["frequency"].get<double>();
    if (j.contains("bandwidth"))
        channel.bandwidth = j["bandwidth"].get<double>();

    NS_LOG_INFO("Channel config parsed: " << channel.propagationModel << ", "
                                           << channel.frequency / 1e9 << " GHz");
}

void
NrSimConfig::ParseMobility(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("defaultModel"))
        mobility.defaultModel = j["defaultModel"].get<std::string>();
    if (j.contains("defaultSpeed"))
        mobility.defaultSpeed = j["defaultSpeed"].get<double>();

    // Parse per-UE waypoints
    if (j.contains("ueWaypoints"))
    {
        ParseUeWaypoints(j["ueWaypoints"]);
    }

    NS_LOG_INFO("Mobility config parsed: default model = " << mobility.defaultModel);
}

void
NrSimConfig::ParseUeWaypoints(const json& j)
{
    NS_LOG_FUNCTION(this);

    // Clear any existing waypoints
    mobility.ueWaypoints.clear();

    // Iterate over UE IDs
    for (auto& [key, value] : j.items())
    {
        try
        {
            // Key is UE ID (string in JSON, convert to uint32_t)
            uint32_t ueId = std::stoul(key);

            UeWaypointConfig config;

            // Get speed
            if (value.contains("speed"))
            {
                config.speed = value["speed"].get<double>();
            }
            else
            {
                config.speed = mobility.defaultSpeed;
            }

            // Get waypoints array
            if (value.contains("waypoints") && value["waypoints"].is_array())
            {
                for (const auto& wp : value["waypoints"])
                {
                    double x = wp["x"].get<double>();
                    double y = wp["y"].get<double>();
                    double z = wp.value("z", 1.5); // Default z = 1.5m

                    config.waypoints.push_back(Vector(x, y, z));
                }
            }

            // Store in map
            if (!config.waypoints.empty())
            {
                mobility.ueWaypoints[ueId] = config;
                NS_LOG_INFO("Loaded " << config.waypoints.size() << " waypoints for UE " << ueId
                                      << " (speed=" << config.speed << " m/s)");
            }
        }
        catch (const std::exception& e)
        {
            NS_LOG_WARN("Failed to parse waypoints for UE " << key << ": " << e.what());
        }
    }

    NS_LOG_INFO("Total UEs with custom waypoints: " << mobility.ueWaypoints.size());
}

void
NrSimConfig::ParseTraffic(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("udpRateDl"))
        traffic.udpRateDl = j["udpRateDl"].get<double>();
    if (j.contains("packetSizeDl"))
        traffic.packetSizeDl = j["packetSizeDl"].get<uint32_t>();
    if (j.contains("udpRateUl"))
        traffic.udpRateUl = j["udpRateUl"].get<double>();
    if (j.contains("packetSizeUl"))
        traffic.packetSizeUl = j["packetSizeUl"].get<uint32_t>();
    
    if (j.contains("enableDownlink"))
        traffic.enableDownlink = j["enableDownlink"].get<bool>();
    if (j.contains("enableUplink"))
        traffic.enableUplink = j["enableUplink"].get<bool>();
    if (j.contains("enableFlowMonitoring"))
        traffic.enableFlowMonitoring = j["enableFlowMonitoring"].get<bool>();

    if (j.contains("startTime"))
        traffic.startTime = j["startTime"].get<double>();
    if (j.contains("duration"))
        traffic.duration = j["duration"].get<double>();

    NS_LOG_INFO("Traffic config parsed: DL=" << traffic.udpRateDl << " Mbps, UL="
                                              << traffic.udpRateUl << " Mbps");
}

void
NrSimConfig::ParseSimulation(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("duration"))
        simDuration = j["duration"].get<double>();

    NS_LOG_INFO("Simulation duration: " << simDuration << " seconds");

    if (j.contains("logTraffic"))
        logTraffic = j["logTraffic"].get<bool>();
}

void
NrSimConfig::ParseMonitoring(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("monitorInterval"))
        monitoring.monitorInterval = j["monitorInterval"].get<double>();

    NS_LOG_INFO("Monitoring config parsed: interval=" << monitoring.monitorInterval << " seconds");
}

void
NrSimConfig::ParseDebug(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("enableDebugLogs"))
        debug.enableDebugLogs = j["enableDebugLogs"].get<bool>();
    if (j.contains("enableVerboseHandoverLogs"))
        debug.enableVerboseHandoverLogs = j["enableVerboseHandoverLogs"].get<bool>();

    NS_LOG_INFO("Debug config parsed: enableDebugLogs=" << (debug.enableDebugLogs ? "true" : "false")
                 << ", enableVerboseHandoverLogs=" << (debug.enableVerboseHandoverLogs ? "true" : "false"));
}

void
NrSimConfig::ParseMetrics(const json& j)
{
    NS_LOG_FUNCTION(this);

    if (j.contains("enableFlowMonitor"))
        enableFlowMonitor = j["enableFlowMonitor"].get<bool>();
    if (j.contains("outputFilePath"))
        outputFilePath = j["outputFilePath"].get<std::string>();

    NS_LOG_INFO("Metrics config parsed: FlowMonitor=" << (enableFlowMonitor ? "enabled" : "disabled")
                                                       << ", outputFilePath=" << outputFilePath);
}

// ========================================================================
// VALIDATION
// ========================================================================

bool
NrSimConfig::Validate() const
{
    NS_LOG_FUNCTION(this);
    bool isValid = true;

    // Topology validation
    if (topology.gnbCount == 0)
    {
        NS_LOG_ERROR("gnbCount must be > 0, got " << topology.gnbCount);
        std::cout << "gnbCount must be > 0, got " << topology.gnbCount << std::endl;
        isValid = false;
    }
    if (topology.ueCount == 0)
    {
        NS_LOG_ERROR("ueCount must be > 0, got " << topology.ueCount);
        std::cout << "ueCount must be > 0, got " << topology.ueCount << std::endl;
        isValid = false;
    }

    // Channel validation
    if (channel.frequency <= 0)
    {
        NS_LOG_ERROR("frequency must be > 0, got " << channel.frequency);
        std::cout << "frequency must be > 0, got " << channel.frequency << std::endl;
        isValid = false;
    }
    if (channel.bandwidth <= 0)
    {
        NS_LOG_ERROR("bandwidth must be > 0, got " << channel.bandwidth);
        std::cout << "bandwidth must be > 0, got " << channel.bandwidth << std::endl;
        isValid = false;
    }

    // Mobility validation
    if (mobility.defaultSpeed < 0)
    {
        NS_LOG_ERROR("defaultSpeed must be >= 0, got " << mobility.defaultSpeed);
        std::cout << "defaultSpeed must be >= 0, got " << mobility.defaultSpeed << std::endl;
        isValid = false;
    }

    // Validate per-UE waypoints
    for (const auto& [ueId, config] : mobility.ueWaypoints)
    {
        if (ueId >= topology.ueCount)
        {
            NS_LOG_WARN("UE ID " << ueId << " has waypoints but exceeds ueCount ("
                                 << topology.ueCount << ")");
        }

        if (config.waypoints.size() < 2)
        {
            NS_LOG_ERROR("UE " << ueId << " has only " << config.waypoints.size()
                                << " waypoints (need at least 2)");
            std::cout << "UE " << ueId << " has only " << config.waypoints.size()
                      << " waypoints (need at least 2)" << std::endl;
            isValid = false;
        }

        if (config.speed <= 0)
        {
            NS_LOG_ERROR("UE " << ueId << " has invalid speed: " << config.speed);
            std::cout << "UE " << ueId << " has invalid speed: " << config.speed << std::endl;
            isValid = false;
        }
    }

    // Traffic validation
    if (traffic.udpRateDl <= 0)
    {
        NS_LOG_ERROR("udpRateDl must be > 0, got " << traffic.udpRateDl);
        std::cout << "udpRateDl must be > 0, got " << traffic.udpRateDl << std::endl;
        isValid = false;
    }

    // Simulation validation
    if (simDuration <= 0)
    {
        NS_LOG_ERROR("simDuration must be > 0, got " << simDuration);
        std::cout << "simDuration must be > 0, got " << simDuration << std::endl;
        isValid = false;
    }

    return isValid;
}

// ========================================================================
// UE WAYPOINT ACCESSORS
// ========================================================================

bool
NrSimConfig::HasUeWaypoints(uint32_t ueId) const
{
    return mobility.ueWaypoints.find(ueId) != mobility.ueWaypoints.end();
}

UeWaypointConfig
NrSimConfig::GetUeWaypoints(uint32_t ueId) const
{
    auto it = mobility.ueWaypoints.find(ueId);
    if (it != mobility.ueWaypoints.end())
    {
        return it->second;
    }

    // Return empty config if not found
    UeWaypointConfig empty;
    empty.speed = mobility.defaultSpeed;
    return empty;
}

// ========================================================================
// PRINTING
// ========================================================================

void
NrSimConfig::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this);

    os << "\n"
       << "╔════════════════════════════════════════════════════════════════╗\n"
       << "║              NR SIMULATION CONFIGURATION                       ║\n"
       << "╚════════════════════════════════════════════════════════════════╝\n"
       << "\n"
       << "┌─ TOPOLOGY ─────────────────────────────────────────────────────┐\n"
       << "│ gNB Count:          " << topology.gnbCount << "\n"
       << "│ UE Count:           " << topology.ueCount << "\n"
       << "│ Area Size:          " << topology.areaSize << " m\n"
       << "│ Use File Positions: " << (topology.useFilePositions ? "Yes" : "No") << "\n";

    if (topology.useFilePositions)
    {
        os << "│ Position File:      " << topology.positionFile << "\n";
        os << "│ Strict File Mode:   " << (topology.strictFileMode ? "Yes" : "No") << "\n";
    }

    if (!topology.useFilePositions)
    {
        os << "│ Placement Strategy: " << topology.uePlacementStrategy << "\n";
    }

    os << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n"
       << "┌─ CHANNEL ──────────────────────────────────────────────────────┐\n"
       << "│ Propagation Model:  " << channel.propagationModel << "\n"
       << "│ Frequency:          " << channel.frequency / 1e9 << " GHz\n"
       << "│ Bandwidth:          " << channel.bandwidth / 1e6 << " MHz\n"
       << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n"
       << "┌─ MOBILITY ─────────────────────────────────────────────────────┐\n"
       << "│ Default Model:      " << mobility.defaultModel << "\n"
       << "│ Default Speed:      " << mobility.defaultSpeed << " m/s\n"
       << "│ UEs with Waypoints: " << mobility.ueWaypoints.size() << "\n";

    // Show waypoint details
    if (!mobility.ueWaypoints.empty())
    {
        os << "│\n";
        os << "│ Waypoint Details:\n";
        for (const auto& [ueId, config] : mobility.ueWaypoints)
        {
            os << "│   UE " << ueId << ": " << config.waypoints.size() << " waypoints, speed="
               << config.speed << " m/s\n";
        }
    }

    os << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n"
       << "┌─ TRAFFIC ──────────────────────────────────────────────────────┐\n"
       << "│ DL Rate:            " << traffic.udpRateDl << " Mbps\n"
       << "│ DL Packet Size:     " << traffic.packetSizeDl << " bytes\n"
       << "│ UL Rate:            " << traffic.udpRateUl << " Mbps\n"
       << "│ UL Packet Size:     " << traffic.packetSizeUl << " bytes\n"
       << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n"
       << "┌─ SIMULATION ───────────────────────────────────────────────────┐\n"
       << "│ Duration:           " << simDuration << " seconds\n"
       << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n"
       << "┌─ METRICS ──────────────────────────────────────────────────────┐\n"
       << "│ Flow Monitor:       " << (enableFlowMonitor ? "Enabled" : "Disabled") << "\n"
       << "│ Output Path:        " << outputFilePath << "\n"
       << "└────────────────────────────────────────────────────────────────┘\n"
       << "\n";
}

} // namespace ns3