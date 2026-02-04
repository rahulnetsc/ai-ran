/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-config-manager.h"

#include "ns3/log.h"
#include "ns3/abort.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrConfigManager");
NS_OBJECT_ENSURE_REGISTERED(NrConfigManager);

TypeId
NrConfigManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrConfigManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrConfigManager>();
    return tid;
}

NrConfigManager::NrConfigManager()
{
    NS_LOG_FUNCTION(this);
}

NrConfigManager::~NrConfigManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrConfigManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

// ========================================================================
// PRIMARY METHODS
// ========================================================================

Ptr<NrSimConfig>
NrConfigManager::LoadFromFile(const std::string& filePath)
{
    NS_LOG_FUNCTION(this << filePath);
    std::cout << "Loading configuration from file: " << filePath << std::endl;
    // Check if file exists
    NS_ABORT_MSG_IF(!FileExists(filePath), "Configuration file not found: " << filePath);

    // Create config object
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();

    // Load from JSON (NrSimConfig handles all parsing)
    bool success = config->LoadFromJson(filePath);
    
    if (!success)
    {
        NS_LOG_ERROR("Failed to load configuration from: " << filePath);
        NS_ABORT_MSG("JSON parsing failed. Check file format.");
    }

    NS_LOG_INFO("Successfully loaded configuration from: " << filePath);

    return config;
}

bool
NrConfigManager::Validate(const Ptr<NrSimConfig>& config) const
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(config == nullptr, "Cannot validate null config");

    bool valid = true;

    // Use config's own validation first
    if (!config->Validate())
    {
        NS_LOG_ERROR("Built-in config validation failed");
        valid = false;
    }

    // Additional file existence checks
    if (config->topology.useFilePositions)
    {
        if (config->topology.positionFile.empty())
        {
            NS_LOG_ERROR("useFilePositions is true but positionFile is empty");
            std::cout << "useFilePositions is true but positionFile is empty" << std::endl;
            valid = false;
        }
        else if (!FileExists(config->topology.positionFile))
        {
            NS_LOG_ERROR("Position file does not exist: " << config->topology.positionFile);
            std::cout << "Position file does not exist: " << config->topology.positionFile << std::endl;
            valid = false;
        }
    }

    // Validate per-UE waypoint files (if any external waypoint files are referenced)
    
    // Validate output directory exists (optional - will be created if needed)
    // Could add directory creation logic here

    if (valid)
    {
        NS_LOG_INFO("Configuration validation: PASSED");
    }
    else
    {
        NS_LOG_ERROR("Configuration validation: FAILED");
    }

    return valid;
}

void
NrConfigManager::ValidateOrAbort(const Ptr<NrSimConfig>& config) const
{
    NS_LOG_FUNCTION(this);

    if (!Validate(config))
    {
        NS_ABORT_MSG("Configuration validation failed! Check logs for details.");
    }
}

// ========================================================================
// UTILITY METHODS
// ========================================================================

void
NrConfigManager::SaveToJson(const Ptr<NrSimConfig>& config, const std::string& filePath) const
{
    NS_LOG_FUNCTION(this << filePath);

    NS_ABORT_MSG_IF(config == nullptr, "Cannot save null config");

    try
    {
        json j;

        // Topology section
        j["topology"]["gnbCount"] = config->topology.gnbCount;
        j["topology"]["ueCount"] = config->topology.ueCount;
        j["topology"]["useFilePositions"] = config->topology.useFilePositions;
        j["topology"]["positionFile"] = config->topology.positionFile;
        j["topology"]["areaSize"] = config->topology.areaSize;
        j["topology"]["strictFileMode"] = config->topology.strictFileMode;
        j["topology"]["uePlacementStrategy"] = config->topology.uePlacementStrategy;
        j["topology"]["numHotspots"] = config->topology.numHotspots;
        j["topology"]["hotspotRadius"] = config->topology.hotspotRadius;
        j["topology"]["numClusters"] = config->topology.numClusters;
        j["topology"]["clusterRadius"] = config->topology.clusterRadius;
        j["topology"]["uesPerCluster"] = config->topology.uesPerCluster;
        j["topology"]["gridSpacing"] = config->topology.gridSpacing;

        // NEW: Serialize position arrays (if present)
        if (!config->topology.gnbPositions.empty())
        {
            json gnbPosArray = json::array();
            for (uint32_t i = 0; i < config->topology.gnbPositions.size(); ++i)
            {
                const Vector& pos = config->topology.gnbPositions[i];
                gnbPosArray.push_back({
                    {"id", i},
                    {"x", pos.x},
                    {"y", pos.y},
                    {"z", pos.z}
                });
            }
            j["topology"]["gnbPositions"] = gnbPosArray;
        }
        
        if (!config->topology.uePositions.empty())
        {
            json uePosArray = json::array();
            for (uint32_t i = 0; i < config->topology.uePositions.size(); ++i)
            {
                const Vector& pos = config->topology.uePositions[i];
                uePosArray.push_back({
                    {"id", i},
                    {"x", pos.x},
                    {"y", pos.y},
                    {"z", pos.z}
                });
            }
            j["topology"]["uePositions"] = uePosArray;
        }

        // Channel section
        j["channel"]["propagationModel"] = config->channel.propagationModel;
        j["channel"]["frequency"] = config->channel.frequency;
        j["channel"]["bandwidth"] = config->channel.bandwidth;

        // Mobility section
        j["mobility"]["defaultModel"] = config->mobility.defaultModel;
        j["mobility"]["defaultSpeed"] = config->mobility.defaultSpeed;

        // Per-UE waypoints
        json ueWaypoints = json::object();
        for (const auto& [ueId, wpConfig] : config->mobility.ueWaypoints)
        {
            json ueData;
            ueData["speed"] = wpConfig.speed;

            json waypointsArray = json::array();
            for (const auto& wp : wpConfig.waypoints)
            {
                waypointsArray.push_back({{"x", wp.x}, {"y", wp.y}, {"z", wp.z}});
            }
            ueData["waypoints"] = waypointsArray;

            ueWaypoints[std::to_string(ueId)] = ueData;
        }
        j["mobility"]["ueWaypoints"] = ueWaypoints;

        // Traffic section
        j["traffic"]["udpRateDl"] = config->traffic.udpRateDl;
        j["traffic"]["packetSizeDl"] = config->traffic.packetSizeDl;
        j["traffic"]["udpRateUl"] = config->traffic.udpRateUl;
        j["traffic"]["packetSizeUl"] = config->traffic.packetSizeUl;

        // Simulation section
        j["simulation"]["duration"] = config->simDuration;

        // Metrics section
        j["metrics"]["enableFlowMonitor"] = config->enableFlowMonitor;
        j["metrics"]["outputFilePath"] = config->outputFilePath;

        // Write to file with pretty printing
        std::ofstream file(filePath);
        NS_ABORT_MSG_IF(!file.is_open(), "Failed to open file for writing: " << filePath);

        file << j.dump(2); // 2-space indentation
        file.close();

        NS_LOG_INFO("Configuration saved to: " << filePath);
    }
    catch (const json::exception& e)
    {
        NS_LOG_ERROR("JSON serialization error: " << e.what());
        NS_ABORT_MSG("Failed to save configuration to JSON");
    }
}

Ptr<NrSimConfig>
NrConfigManager::CreateDefaultConfig() const
{
    NS_LOG_FUNCTION(this);

    // Simple! Just create - all defaults are in NrSimConfig
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();

    NS_LOG_INFO("Created default configuration");

    return config;
}

void
NrConfigManager::PrintConfigSummary(const Ptr<NrSimConfig>& config, std::ostream& os) const
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(config == nullptr, "Cannot print null config");

    // Use NrSimConfig's built-in Print method
    config->Print(os);
}

// ========================================================================
// PRIVATE HELPERS
// ========================================================================

bool
NrConfigManager::FileExists(const std::string& filePath) const
{
    std::ifstream file(filePath);
    return file.good();
}

} // namespace ns3