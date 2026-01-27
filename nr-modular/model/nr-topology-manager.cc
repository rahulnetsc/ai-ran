/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-topology-manager.h"
#include "utils/nr-sim-config.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/mobility-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include "ns3/rectangle.h"
#include "ns3/string.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/waypoint-mobility-model.h"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrTopologyManager");
NS_OBJECT_ENSURE_REGISTERED(NrTopologyManager);

TypeId
NrTopologyManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrTopologyManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrTopologyManager>();
    return tid;
}

NrTopologyManager::NrTopologyManager()
    : m_config(nullptr),
      m_deployed(false)
{
    NS_LOG_FUNCTION(this);
}

NrTopologyManager::~NrTopologyManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrTopologyManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_config = nullptr;
    m_deployed = false;
    m_gnbNodes = NodeContainer();
    m_ueNodes = NodeContainer();
    m_gnbPositions.clear();
    m_uePositions.clear();
    Object::DoDispose();
}

void
NrTopologyManager::SetConfig(const Ptr<NrSimConfig>& config)
{
    NS_LOG_FUNCTION(this << config);
    NS_ABORT_MSG_IF(config == nullptr, "NrTopologyManager: config cannot be null");
    m_config = config;
}

void
NrTopologyManager::DeployTopology()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_config == nullptr, "Configuration must be set before deployment");
    NS_ABORT_MSG_IF(m_deployed, "Topology has already been deployed");

    uint32_t numGnbs = m_config->topology.gnbCount;
    uint32_t numUes = m_config->topology.ueCount;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Deploying network topology" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Creating " << numGnbs << " gNBs and " << numUes << " UEs" << std::endl;
    std::cout << "Placement strategy: " << m_config->topology.uePlacementStrategy << std::endl;
    std::cout << "========================================" << std::endl;

    // Create nodes
    m_gnbNodes.Create(numGnbs);
    m_ueNodes.Create(numUes);

    // ====================================================================
    // SMART MOBILITY INSTALLATION
    // Install the correct mobility model for each UE based on config
    // ====================================================================
    
    MobilityHelper mobility;
    
    // gNBs: Always ConstantPosition (stationary)
    std::cout << "\nInstalling ConstantPositionMobilityModel for " << numGnbs << " gNBs" << std::endl;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_gnbNodes);
    
    // UEs: Install appropriate model based on config
    std::cout << "Installing mobility models for " << numUes << " UEs:" << std::endl;
    
    // Get mobility parameters from config
    double areaSize = m_config->topology.areaSize;
    double bounds = areaSize / 2.0;
    double speed = m_config->mobility.defaultSpeed;
    std::string defaultModel = m_config->mobility.defaultModel;
    
    uint32_t waypointUes = 0;
    uint32_t randomWalkUes = 0;
    uint32_t staticUes = 0;
    
    for (uint32_t ueId = 0; ueId < numUes; ++ueId)
    {
        Ptr<Node> ueNode = m_ueNodes.Get(ueId);
        
        if (m_config->HasUeWaypoints(ueId))
        {
            // This UE has waypoints → WaypointMobilityModel
            // Waypoints will be added later by MobilityManager
            mobility.SetMobilityModel("ns3::WaypointMobilityModel");
            mobility.Install(ueNode);
            waypointUes++;
        }
        else if (defaultModel == "RandomWalk" || defaultModel == "RandomWalk2d")
        {
            // RandomWalk → Configure and install
            mobility.SetMobilityModel(
                "ns3::RandomWalk2dMobilityModel",
                "Bounds", RectangleValue(Rectangle(-bounds, bounds, -bounds, bounds)),
                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]"),
                "Distance", DoubleValue(50.0)
            );
            mobility.Install(ueNode);
            randomWalkUes++;
        }
        else
        {
            // Static or unknown → ConstantPosition
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(ueNode);
            staticUes++;
        }
    }
    
    std::cout << "Mobility models installed:" << std::endl;
    std::cout << "  Waypoint: " << waypointUes << " UEs" << std::endl;
    std::cout << "  RandomWalk: " << randomWalkUes << " UEs" << std::endl;
    std::cout << "  Static: " << staticUes << " UEs" << std::endl;

    // Deploy positions based on configuration
    if (m_config->topology.useFilePositions)
    {
        DeployFromFile();
    }
    else
    {
        DeployHexagonal();
    }

    // IMPORTANT: Override initial positions for UEs with waypoints
    // This ensures UEs start at the first waypoint of their path
    SetInitialPositionsFromWaypoints();
    
    m_deployed = true;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Topology deployment complete" << std::endl;
    std::cout << "  Total gNBs: " << m_gnbNodes.GetN() << std::endl;
    std::cout << "  Total UEs: " << m_ueNodes.GetN() << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void
NrTopologyManager::DeployFromFile()
{
    NS_LOG_FUNCTION(this);
    
    std::cout << "\n--- Deploying from file ---" << std::endl;
    std::string filename = m_config->topology.positionFile;
    std::cout << "Reading positions from: " << filename << std::endl;
    
    std::ifstream file(filename);
    if (!file.is_open())
    {
        NS_ABORT_MSG("Failed to open position file: " << filename);
    }
    
    // Read gNB positions
    uint32_t numGnbs = m_gnbNodes.GetN();
    for (uint32_t i = 0; i < numGnbs; ++i)
    {
        double x, y, z;
        if (!(file >> x >> y >> z))
        {
            NS_ABORT_MSG("Not enough gNB positions in file (need " << numGnbs << ")");
        }
        
        Vector pos(x, y, z);
        m_gnbPositions.push_back(pos);
        
        Ptr<MobilityModel> mobility = m_gnbNodes.Get(i)->GetObject<MobilityModel>();
        NS_ASSERT(mobility != nullptr);
        mobility->SetPosition(pos);
        
        std::cout << "  gNB " << i << ": (" << x << ", " << y << ", " << z << ")" << std::endl;
    }
    
    // Read UE positions
    uint32_t numUes = m_ueNodes.GetN();
    for (uint32_t i = 0; i < numUes; ++i)
    {
        double x, y, z;
        if (!(file >> x >> y >> z))
        {
            NS_ABORT_MSG("Not enough UE positions in file (need " << numUes << ")");
        }
        
        Vector pos(x, y, z);
        m_uePositions.push_back(pos);
        
        Ptr<MobilityModel> mobility = m_ueNodes.Get(i)->GetObject<MobilityModel>();
        NS_ASSERT(mobility != nullptr);
        mobility->SetPosition(pos);
        
        std::cout << "  UE " << i << ": (" << x << ", " << y << ", " << z << ")" << std::endl;
    }
    
    file.close();
    std::cout << "File-based deployment complete" << std::endl;
}

void
NrTopologyManager::DeployHexagonal()
{
    NS_LOG_FUNCTION(this);
    
    std::cout << "\n--- Deploying hexagonal layout ---" << std::endl;
    
    double areaSize = m_config->topology.areaSize;
    double gnbHeight = 25.0;
    double ueHeight = 1.5;
    
    // ====================================================================
    // HEXAGONAL gNB DEPLOYMENT
    // Standard 3GPP hexagonal layout with ISD (Inter-Site Distance)
    // ====================================================================
    
    uint32_t numGnbs = m_gnbNodes.GetN();
    double centerX = areaSize / 2.0;
    double centerY = areaSize / 2.0;
    
    // Inter-Site Distance (adjust based on scenario)
    // UMa: 500m typical, UMi: 200m typical
    double isd = (m_config->channel.propagationModel == "UMa") ? 500.0 : 200.0;
    
    std::cout << "gNB deployment:" << std::endl;
    std::cout << "  Scenario: " << m_config->channel.propagationModel << std::endl;
    std::cout << "  Inter-Site Distance (ISD): " << isd << " m" << std::endl;
    std::cout << "  Pattern: " << (numGnbs == 1 ? "Single site" : 
                                   numGnbs == 7 ? "1 center + 6 ring" :
                                   numGnbs == 19 ? "1 center + 6 inner + 12 outer" :
                                   "Custom") << std::endl;
    
    if (numGnbs == 1)
    {
        // Single gNB at center
        Vector pos(centerX, centerY, gnbHeight);
        m_gnbPositions.push_back(pos);
        
        Ptr<MobilityModel> mobility = m_gnbNodes.Get(0)->GetObject<MobilityModel>();
        NS_ASSERT(mobility != nullptr);
        mobility->SetPosition(pos);
        
        std::cout << "  gNB 0: (" << pos.x << ", " << pos.y << ", " << pos.z 
                  << ") [center]" << std::endl;
    }
    else if (numGnbs == 7)
    {
        // 1 center + 6 surrounding in hexagonal pattern
        // Center gNB
        Vector centerPos(centerX, centerY, gnbHeight);
        m_gnbPositions.push_back(centerPos);
        
        Ptr<MobilityModel> mobility = m_gnbNodes.Get(0)->GetObject<MobilityModel>();
        NS_ASSERT(mobility != nullptr);
        mobility->SetPosition(centerPos);
        
        std::cout << "  gNB 0: (" << centerPos.x << ", " << centerPos.y << ", " 
                  << centerPos.z << ") [center]" << std::endl;
        
        // 6 surrounding gNBs in hexagonal pattern
        for (uint32_t i = 1; i < 7; ++i)
        {
            double angle = (i - 1) * M_PI / 3.0;  // 60 degrees apart
            double x = centerX + isd * std::cos(angle);
            double y = centerY + isd * std::sin(angle);
            
            Vector pos(x, y, gnbHeight);
            m_gnbPositions.push_back(pos);
            
            mobility = m_gnbNodes.Get(i)->GetObject<MobilityModel>();
            NS_ASSERT(mobility != nullptr);
            mobility->SetPosition(pos);
            
            std::cout << "  gNB " << i << ": (" 
                      << std::fixed << std::setprecision(2)
                      << pos.x << ", " << pos.y << ", " << pos.z 
                      << ") [ring, angle=" << (angle * 180.0 / M_PI) << "°]" << std::endl;
        }
    }
    else if (numGnbs == 19)
    {
        // 1 center + 6 inner ring + 12 outer ring (2-tier hexagonal)
        
        // Center gNB
        Vector centerPos(centerX, centerY, gnbHeight);
        m_gnbPositions.push_back(centerPos);
        
        Ptr<MobilityModel> mobility = m_gnbNodes.Get(0)->GetObject<MobilityModel>();
        NS_ASSERT(mobility != nullptr);
        mobility->SetPosition(centerPos);
        
        std::cout << "  gNB 0: (" << centerPos.x << ", " << centerPos.y << ", " 
                  << centerPos.z << ") [center]" << std::endl;
        
        // Inner ring (6 gNBs at distance ISD)
        for (uint32_t i = 1; i < 7; ++i)
        {
            double angle = (i - 1) * M_PI / 3.0;
            double x = centerX + isd * std::cos(angle);
            double y = centerY + isd * std::sin(angle);
            
            Vector pos(x, y, gnbHeight);
            m_gnbPositions.push_back(pos);
            
            mobility = m_gnbNodes.Get(i)->GetObject<MobilityModel>();
            NS_ASSERT(mobility != nullptr);
            mobility->SetPosition(pos);
            
            std::cout << "  gNB " << i << ": (" 
                      << std::fixed << std::setprecision(2)
                      << pos.x << ", " << pos.y << ", " << pos.z 
                      << ") [inner ring]" << std::endl;
        }
        
        // Outer ring (12 gNBs at distance sqrt(3)*ISD)
        double outerRadius = std::sqrt(3.0) * isd;
        for (uint32_t i = 7; i < 19; ++i)
        {
            double angle = (i - 7) * M_PI / 6.0;  // 30 degrees apart
            double x = centerX + outerRadius * std::cos(angle);
            double y = centerY + outerRadius * std::sin(angle);
            
            Vector pos(x, y, gnbHeight);
            m_gnbPositions.push_back(pos);
            
            mobility = m_gnbNodes.Get(i)->GetObject<MobilityModel>();
            NS_ASSERT(mobility != nullptr);
            mobility->SetPosition(pos);
            
            std::cout << "  gNB " << i << ": (" 
                      << std::fixed << std::setprecision(2)
                      << pos.x << ", " << pos.y << ", " << pos.z 
                      << ") [outer ring]" << std::endl;
        }
    }
    else
    {
        // Custom: spread gNBs in a grid pattern
        std::cout << "  Warning: Non-standard gNB count (" << numGnbs 
                  << "), using grid deployment" << std::endl;
        
        uint32_t gridSize = std::ceil(std::sqrt(numGnbs));
        double spacing = isd;
        double startX = centerX - (gridSize - 1) * spacing / 2.0;
        double startY = centerY - (gridSize - 1) * spacing / 2.0;
        
        uint32_t idx = 0;
        for (uint32_t row = 0; row < gridSize && idx < numGnbs; ++row)
        {
            for (uint32_t col = 0; col < gridSize && idx < numGnbs; ++col)
            {
                double x = startX + col * spacing;
                double y = startY + row * spacing;
                
                Vector pos(x, y, gnbHeight);
                m_gnbPositions.push_back(pos);
                
                Ptr<MobilityModel> mobility = m_gnbNodes.Get(idx)->GetObject<MobilityModel>();
                NS_ASSERT(mobility != nullptr);
                mobility->SetPosition(pos);
                
                std::cout << "  gNB " << idx << ": (" 
                          << std::fixed << std::setprecision(2)
                          << pos.x << ", " << pos.y << ", " << pos.z 
                          << ") [grid]" << std::endl;
                idx++;
            }
        }
    }
    
    // ====================================================================
    // UE DEPLOYMENT
    // ====================================================================
    
    uint32_t numUes = m_ueNodes.GetN();
    std::string strategy = m_config->topology.uePlacementStrategy;
    
    std::cout << "\nUE deployment:" << std::endl;
    std::cout << "  Strategy: " << strategy << std::endl;
    std::cout << "  Count: " << numUes << std::endl;
    
    if (strategy == "uniform" || strategy == "random")
    {
        // Random uniform distribution across area
        Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable>();
        randX->SetAttribute("Min", DoubleValue(0.0));
        randX->SetAttribute("Max", DoubleValue(areaSize));
        
        Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable>();
        randY->SetAttribute("Min", DoubleValue(0.0));
        randY->SetAttribute("Max", DoubleValue(areaSize));
        
        for (uint32_t i = 0; i < numUes; ++i)
        {
            Vector pos(randX->GetValue(), randY->GetValue(), ueHeight);
            m_uePositions.push_back(pos);
            
            Ptr<MobilityModel> mobility = m_ueNodes.Get(i)->GetObject<MobilityModel>();
            NS_ASSERT(mobility != nullptr);
            mobility->SetPosition(pos);
            
            if (i < 5 || i >= numUes - 2)  // Show first 5 and last 2
            {
                std::cout << "  UE " << i << ": (" 
                          << std::fixed << std::setprecision(2)
                          << pos.x << ", " << pos.y << ", " << pos.z << ")";
                
                // Find nearest gNB
                double minDist = std::numeric_limits<double>::max();
                uint32_t nearestGnb = 0;
                for (uint32_t g = 0; g < m_gnbPositions.size(); ++g)
                {
                    double dx = pos.x - m_gnbPositions[g].x;
                    double dy = pos.y - m_gnbPositions[g].y;
                    double dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        nearestGnb = g;
                    }
                }
                std::cout << " [nearest: gNB " << nearestGnb 
                          << ", " << minDist << "m]" << std::endl;
            }
            else if (i == 5)
            {
                std::cout << "  ... (" << (numUes - 7) << " more UEs) ..." << std::endl;
            }
        }
    }
    else if (strategy == "grid")
    {
        // Grid placement
        double spacing = m_config->topology.gridSpacing;
        uint32_t gridSize = std::ceil(std::sqrt(numUes));
        
        uint32_t idx = 0;
        for (uint32_t row = 0; row < gridSize && idx < numUes; ++row)
        {
            for (uint32_t col = 0; col < gridSize && idx < numUes; ++col)
            {
                Vector pos(col * spacing, row * spacing, ueHeight);
                m_uePositions.push_back(pos);
                
                Ptr<MobilityModel> mobility = m_ueNodes.Get(idx)->GetObject<MobilityModel>();
                NS_ASSERT(mobility != nullptr);
                mobility->SetPosition(pos);
                
                if (idx < 3 || idx >= numUes - 2)
                {
                    std::cout << "  UE " << idx << ": (" 
                              << std::fixed << std::setprecision(2)
                              << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                }
                else if (idx == 3)
                {
                    std::cout << "  ... (" << (numUes - 5) << " more UEs) ..." << std::endl;
                }
                idx++;
            }
        }
    }
    else
    {
        // Circle around center gNB (default)
        double radius = isd * 0.7;  // 70% of ISD
        
        for (uint32_t i = 0; i < numUes; ++i)
        {
            double angle = (2.0 * M_PI * i) / numUes;
            Vector pos(centerX + radius * std::cos(angle),
                      centerY + radius * std::sin(angle),
                      ueHeight);
            m_uePositions.push_back(pos);
            
            Ptr<MobilityModel> mobility = m_ueNodes.Get(i)->GetObject<MobilityModel>();
            NS_ASSERT(mobility != nullptr);
            mobility->SetPosition(pos);
            
            if (i < 3 || i >= numUes - 2)
            {
                std::cout << "  UE " << i << ": (" 
                          << std::fixed << std::setprecision(2)
                          << pos.x << ", " << pos.y << ", " << pos.z 
                          << ") [angle=" << (angle * 180.0 / M_PI) << "°]" << std::endl;
            }
            else if (i == 3)
            {
                std::cout << "  ... (" << (numUes - 5) << " more UEs) ..." << std::endl;
            }
        }
    }
    
    std::cout << "Hexagonal deployment complete" << std::endl;
}

void
NrTopologyManager::SetInitialPositionsFromWaypoints()
{
    NS_LOG_FUNCTION(this);
    
    std::cout << "\n--- Setting initial positions from waypoints ---" << std::endl;
    
    uint32_t numUes = m_ueNodes.GetN();
    uint32_t waypointOverrides = 0;
    
    for (uint32_t ueId = 0; ueId < numUes; ++ueId)
    {
        if (m_config->HasUeWaypoints(ueId))
        {
            UeWaypointConfig wpConfig = m_config->GetUeWaypoints(ueId);
            
            if (!wpConfig.waypoints.empty())
            {
                // Set UE position to first waypoint
                Vector firstWaypoint = wpConfig.waypoints[0];
                
                Ptr<MobilityModel> mobility = m_ueNodes.Get(ueId)->GetObject<MobilityModel>();
                NS_ASSERT(mobility != nullptr);
                mobility->SetPosition(firstWaypoint);
                
                // Update stored position
                if (ueId < m_uePositions.size())
                {
                    m_uePositions[ueId] = firstWaypoint;
                }
                
                waypointOverrides++;
                
                std::cout << "  UE " << ueId << " initial position set to first waypoint: ("
                          << std::fixed << std::setprecision(2)
                          << firstWaypoint.x << ", " << firstWaypoint.y << ", " 
                          << firstWaypoint.z << ")" << std::endl;
            }
        }
    }
    
    if (waypointOverrides > 0)
    {
        std::cout << "Overrode initial positions for " << waypointOverrides 
                  << " waypoint UEs" << std::endl;
    }
    else
    {
        std::cout << "No waypoint UEs found, using default positions" << std::endl;
    }
}

NodeContainer
NrTopologyManager::GetGnbNodes() const
{
    return m_gnbNodes;
}

NodeContainer
NrTopologyManager::GetUeNodes() const
{
    return m_ueNodes;
}

bool
NrTopologyManager::IsDeployed() const
{
    return m_deployed;
}

uint32_t
NrTopologyManager::GetNumGnbs() const
{
    return m_gnbNodes.GetN();
}

uint32_t
NrTopologyManager::GetNumUes() const
{
    return m_ueNodes.GetN();
}

std::vector<Vector>
NrTopologyManager::GetGnbPositions() const
{
    return m_gnbPositions;
}

std::vector<Vector>
NrTopologyManager::GetUePositions() const
{
    return m_uePositions;
}

} // namespace ns3