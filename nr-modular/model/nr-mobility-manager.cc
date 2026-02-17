/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-mobility-manager.h"
#include "utils/nr-sim-config.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/mobility-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/rectangle.h"

#include <cmath>
#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMobilityManager");
NS_OBJECT_ENSURE_REGISTERED(NrMobilityManager);

TypeId
NrMobilityManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrMobilityManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrMobilityManager>();
    return tid;
}

NrMobilityManager::NrMobilityManager()
    : m_config(nullptr),
      m_installed(false)
{
    NS_LOG_FUNCTION(this);
}

NrMobilityManager::~NrMobilityManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrMobilityManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_config = nullptr;
    m_installed = false;
    Object::DoDispose();
}

void
NrMobilityManager::SetConfig(const Ptr<NrSimConfig>& config)
{
    NS_LOG_FUNCTION(this << config);
    NS_ABORT_MSG_IF(config == nullptr, "NrMobilityManager: config cannot be null");
    m_config = config;
}

void
NrMobilityManager::InstallGnbMobility(const NodeContainer& gnbNodes)
{
    NS_LOG_FUNCTION(this << gnbNodes.GetN());
    NS_ABORT_MSG_IF(m_config == nullptr, "Config must be set before installing mobility");
    // Check if already installed: Should be already installed from Topology Manager
    // getting mobility model from each gNB node
    for (uint32_t gnbId = 0; gnbId < gnbNodes.GetN(); ++gnbId)
    {
        Ptr<Node> gnbNode = gnbNodes.Get(gnbId);
        Ptr<MobilityModel> mob = gnbNode->GetObject<MobilityModel>();
        NS_ABORT_MSG_IF(mob == nullptr, "No mobility model on gNB " << gnbId);
    }
}

void
NrMobilityManager::InstallUeMobility(const NodeContainer& ueNodes)
{
    NS_LOG_FUNCTION(this << ueNodes.GetN());
    NS_ABORT_MSG_IF(m_config == nullptr, "Config must be set before installing mobility");
    NS_ABORT_MSG_IF(m_installed, "Mobility already installed");

    std::cout << "========================================" << std::endl;
    std::cout << "Installing UE mobility" << std::endl;
    std::cout << "Total UEs: " << ueNodes.GetN() << std::endl;
    std::cout << "Default model: " << m_config->mobility.defaultModel << std::endl;
    std::cout << "Default speed: " << m_config->mobility.defaultSpeed << " m/s" << std::endl;
    std::cout << "UEs with waypoints: " << m_config->mobility.ueWaypoints.size() << std::endl;
    std::cout << "========================================" << std::endl;

    uint32_t waypointCount = 0;
    uint32_t randomWalkCount = 0;
    uint32_t staticCount = 0;

    for (uint32_t ueId = 0; ueId < ueNodes.GetN(); ++ueId)
    {

        Ptr<Node> ueNode = ueNodes.Get(ueId);

        // Check if this UE has waypoints defined
        if (m_config->HasUeWaypoints(ueId))
        {
            UeWaypointConfig wpConfig = m_config->GetUeWaypoints(ueId);

            if (wpConfig.waypoints.size() >= 2)
            {
                InstallWaypointMobilityFromConfig(ueNode, ueId);
                waypointCount++;
                
                std::cout << "  UE " << ueId << ": Waypoint mobility (" 
                          << wpConfig.waypoints.size() << " points, "
                          << wpConfig.speed << " m/s)" << std::endl;
            }
            else
            {
                InstallRandomWalkMobility(ueNode);
                randomWalkCount++;
            }
        }
        else
        {
            std::string defaultModel = m_config->mobility.defaultModel;
            if (defaultModel == "Static" || defaultModel == "ConstantPosition" || defaultModel == "RandomWalk" || defaultModel == "RandomWalk2d")
            {
                Ptr<MobilityModel> mob = ueNode->GetObject<MobilityModel>();
                NS_ABORT_MSG_IF(mob == nullptr, "No mobility model on UE " << ueId);
                staticCount++;
            }
            else
            {
                InstallRandomWalkMobility(ueNode);
                randomWalkCount++;
            }
            
        }
    }

    m_installed = true;

    std::cout << "========================================" << std::endl;
    std::cout << "UE mobility installation complete:" << std::endl;
    std::cout << "  Waypoint mobility: " << waypointCount << " UEs" << std::endl;
    std::cout << "  RandomWalk mobility: " << randomWalkCount << " UEs" << std::endl;
    std::cout << "  Static mobility: " << staticCount << " UEs" << std::endl;
    std::cout << "========================================" << std::endl;
}

void
NrMobilityManager::InstallStaticMobility(Ptr<Node> node, const Vector& position)
{
    NS_LOG_FUNCTION(this << node << position);
    
    // ConstantPositionMobilityModel already installed by Topology Manager
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(mob == nullptr, "No mobility model found on node!");
}

void
NrMobilityManager::InstallRandomWalkMobility(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    
    // RandomWalk2dMobilityModel already installed by Topology Manager
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(mob == nullptr, "No mobility model found on node!");
}

void
NrMobilityManager::InstallWaypointMobilityFromConfig(Ptr<Node> node, uint32_t ueId)
{
    NS_LOG_FUNCTION(this << node << ueId);

    UeWaypointConfig wpConfig = m_config->GetUeWaypoints(ueId);
    
    NS_ABORT_MSG_IF(wpConfig.waypoints.size() < 2,
                    "UE " << ueId << ": Waypoint mobility requires at least 2 waypoints");
    NS_ABORT_MSG_IF(wpConfig.speed <= 0.0,
                    "UE " << ueId << ": Waypoint mobility requires positive speed");

    std::vector<Waypoint> waypoints = BuildWaypointsWithTiming(wpConfig.waypoints, wpConfig.speed);

    Ptr<WaypointMobilityModel> wpMob = node->GetObject<WaypointMobilityModel>();
    NS_ABORT_MSG_IF(wpMob == nullptr,
                    "UE " << ueId << ": No WaypointMobilityModel found! "
                    << "Topology Manager should have installed it.");

    for (const auto& wp : waypoints)
    {
        wpMob->AddWaypoint(wp);
    }
}

std::vector<Waypoint>
NrMobilityManager::BuildWaypointsWithTiming(const std::vector<Vector>& positions, double speedMps)
{
    NS_LOG_FUNCTION(this << positions.size() << speedMps);

    std::vector<Waypoint> result;
    Time currentTime = Seconds(0.0);

    // First waypoint at t=0
    result.emplace_back(currentTime, positions[0]);

    // Calculate arrival time for each subsequent waypoint
    for (size_t i = 1; i < positions.size(); ++i)
    {
        Vector from = positions[i - 1];
        Vector to = positions[i];

        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double dz = to.z - from.z;
        double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        double travelTime = distance / speedMps;
        currentTime += Seconds(travelTime);

        result.emplace_back(currentTime, to);
    }

    return result;
}

} // namespace ns3