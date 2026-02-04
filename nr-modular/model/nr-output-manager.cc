/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-output-manager.h"
#include "utils/nr-sim-config.h"
#include "nr-topology-manager.h"
#include "nr-network-manager.h"
#include "nr-traffic-manager.h"
#include "nr-metrics-manager.h"
#include "nr-channel-manager.h"
#include "nr-mobility-manager.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/mobility-model.h"
#include "ns3/waypoint-mobility-model.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-ue-phy.h"
#include "ns3/nr-ue-mac.h"
#include "ns3/nr-gnb-mac.h"
#include "ns3/nr-mac-scheduler.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("NrOutputManager");
NS_OBJECT_ENSURE_REGISTERED(NrOutputManager);

TypeId
NrOutputManager::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::NrOutputManager")
        .SetParent<Object>()
        .SetGroupName("NrModular")
        .AddConstructor<NrOutputManager>();
    return tid;
}

// ================================================================
// CONSTRUCTOR / DESTRUCTOR
// ================================================================

NrOutputManager::NrOutputManager()
    : m_config(nullptr),
      m_topologyManager(nullptr),
      m_networkManager(nullptr),
      m_trafficManager(nullptr),
      m_metricsManager(nullptr),
      m_channelManager(nullptr),
      m_mobilityManager(nullptr),
      m_telemetryEnabled(false),
      m_telemetryInitialized(false),
      m_publishMethod(PUBLISH_DISABLED),
      m_publishHost("localhost"),
      m_publishPort(5555),
      m_publishFilepath("/tmp/nr_sim_state.json"),
      m_udpSocket(-1),
      m_tcpSocket(-1),
      m_tcpConnected(false),
      m_publishedStateCount(0),
      m_failedPublishCount(0),
      m_publishInterval(Seconds(0.1))
{
    NS_LOG_FUNCTION(this);
    
    // Initialize wall clock start time
    m_wallClockStart = std::chrono::steady_clock::now();
    m_simulationStartTime = Simulator::Now();
}

NrOutputManager::~NrOutputManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrOutputManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    
    // Stop telemetry
    StopTelemetry();
    
    // Close sockets
    if (m_udpSocket >= 0)
    {
        close(m_udpSocket);
        m_udpSocket = -1;
    }
    
    if (m_tcpSocket >= 0)
    {
        close(m_tcpSocket);
        m_tcpSocket = -1;
    }
    
    // Close file
    if (m_outputFile.is_open())
    {
        m_outputFile.close();
    }
    
    // Clear references
    m_config = nullptr;
    m_topologyManager = nullptr;
    m_networkManager = nullptr;
    m_trafficManager = nullptr;
    m_metricsManager = nullptr;
    m_channelManager = nullptr;
    m_mobilityManager = nullptr;
    
    Object::DoDispose();
}

// ================================================================
// CONFIGURATION
// ================================================================

void
NrOutputManager::SetConfig(Ptr<NrSimConfig> config)
{
    NS_LOG_FUNCTION(this);
    m_config = config;
}

void
NrOutputManager::SetManagers(Ptr<NrTopologyManager> topology,
                            Ptr<NrNetworkManager> network,
                            Ptr<NrTrafficManager> traffic,
                            Ptr<NrMetricsManager> metrics,
                            Ptr<NrChannelManager> channel,
                            Ptr<NrMobilityManager> mobility)
{
    NS_LOG_FUNCTION(this);
    
    m_topologyManager = topology;
    m_networkManager = network;
    m_trafficManager = traffic;
    m_metricsManager = metrics;
    m_channelManager = channel;
    m_mobilityManager = mobility;
    
    NS_LOG_INFO("OutputManager: Managers configured");
}

void
NrOutputManager::SetTelemetryConfig(const TelemetryConfig& config)
{
    NS_LOG_FUNCTION(this);
    m_telemetryConfig = config;
}

NrOutputManager::TelemetryConfig
NrOutputManager::GetTelemetryConfig() const
{
    return m_telemetryConfig;
}

// ================================================================
// TRADITIONAL FILE OUTPUT
// ================================================================

void
NrOutputManager::WriteResults()
{
    NS_LOG_FUNCTION(this);
    
    if (m_config == nullptr)
    {
        NS_LOG_WARN("No config set, skipping WriteResults");
        return;
    }
    
    std::string outputPath = m_config->outputFilePath;
    WriteResultsToFile(outputPath);
}

void
NrOutputManager::WriteResultsToFile(const std::string& filepath)
{
    NS_LOG_FUNCTION(this << filepath);
    
    // Collect final state
    SimulationState finalState = CollectCurrentState();
    
    // Generate summary report
    std::string report = GenerateSummaryReport(finalState);
    
    // Write to file
    std::ofstream file(filepath);
    if (file.is_open())
    {
        file << report;
        file.close();
        NS_LOG_INFO("Results written to: " << filepath);
    }
    else
    {
        NS_LOG_ERROR("Failed to open output file: " << filepath);
    }
}

// ================================================================
// TELEMETRY INITIALIZATION
// ================================================================

void
NrOutputManager::InitializeTelemetry()
{
    NS_LOG_FUNCTION(this);
    
    if (m_telemetryInitialized)
    {
        NS_LOG_WARN("Telemetry already initialized");
        return;
    }
    
    // Validate managers are set
    NS_ABORT_MSG_IF(m_topologyManager == nullptr, "TopologyManager not set");
    NS_ABORT_MSG_IF(m_networkManager == nullptr, "NetworkManager not set");
    NS_ABORT_MSG_IF(m_trafficManager == nullptr, "TrafficManager not set");
    
    // Initialize buffers
    m_stateHistory.clear();
    m_handoverEvents.clear();
    m_eventLog.clear();
    
    // Reset statistics
    m_publishedStateCount = 0;
    m_failedPublishCount = 0;
    m_stateGenTimes.clear();
    m_jsonSizes.clear();
    
    m_telemetryInitialized = true;
    
    NS_LOG_INFO("Telemetry initialized");
    std::cout << "✓ Telemetry system initialized" << std::endl;
}

void
NrOutputManager::StartTelemetry(double interval)
{
    NS_LOG_FUNCTION(this << interval);
    
    NS_ABORT_MSG_IF(!m_telemetryInitialized, 
                   "Must call InitializeTelemetry() first");
    
    if (m_telemetryEnabled)
    {
        NS_LOG_WARN("Telemetry already started");
        return;
    }
    
    m_publishInterval = Seconds(interval);
    m_telemetryEnabled = true;
    
    std::cout << "✓ Real-time telemetry started" << std::endl;
    std::cout << "  Update interval: " << interval << " seconds" << std::endl;
    std::cout << "  Method: ";
    
    switch (m_publishMethod)
    {
        case PUBLISH_FILE:
            std::cout << "File (" << m_publishFilepath << ")" << std::endl;
            break;
        case PUBLISH_UDP:
            std::cout << "UDP (" << m_publishHost << ":" << m_publishPort << ")" << std::endl;
            break;
        case PUBLISH_TCP:
            std::cout << "TCP (" << m_publishHost << ":" << m_publishPort << ")" << std::endl;
            break;
        case PUBLISH_PIPE:
            std::cout << "Pipe (" << m_publishFilepath << ")" << std::endl;
            break;
        default:
            std::cout << "Disabled" << std::endl;
    }
    
    // Publish initial state immediately
    std::cout << "  Publishing initial state..." << std::endl;
    PublishStateNow("initial");
    
    // Schedule periodic updates
    ScheduleNextUpdate();
}

void
NrOutputManager::StopTelemetry()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_telemetryEnabled)
        return;
    
    m_telemetryEnabled = false;
    
    // Cancel pending event
    if (m_publishEvent.IsPending())
    {
        Simulator::Cancel(m_publishEvent);
    }
    
    NS_LOG_INFO("Telemetry stopped");
    std::cout << "✓ Telemetry stopped" << std::endl;
}

void
NrOutputManager::PublishStateNow(const std::string& eventType)
{
    NS_LOG_FUNCTION(this << eventType);
    
    if (!m_telemetryInitialized)
    {
        NS_LOG_WARN("Telemetry not initialized, skipping publish");
        return;
    }
    
    // Collect and publish immediately
    SimulationState state = CollectCurrentState();
    PublishState(state, eventType);
}

// ================================================================
// EVENT HANDLERS
// ================================================================

void
NrOutputManager::OnUeAttachment(uint32_t ueId, uint16_t cellId)
{
    NS_LOG_FUNCTION(this << ueId << cellId);
    
    // Log event
    std::ostringstream desc;
    desc << "UE " << ueId << " attached to cell " << cellId;
    LogEvent("attachment", desc.str());
    
    // Trigger update if enabled
    if (m_telemetryConfig.eventTriggeredUpdates && m_telemetryEnabled)
    {
        PublishStateNow("attachment");
    }
}

void
NrOutputManager::OnHandover(uint32_t ueId, uint16_t sourceCellId, 
                           uint16_t targetCellId, bool success)
{
    NS_LOG_FUNCTION(this << ueId << sourceCellId << targetCellId << success);
    
    // Create handover event
    SimulationState::HandoverEvent ho;
    ho.timestamp = Simulator::Now().GetSeconds();
    ho.ueId = ueId;
    ho.sourceCellId = sourceCellId;
    ho.targetCellId = targetCellId;
    ho.success = success;
    ho.reason = "user_triggered";  // Could be enhanced
    
    // Add to history
    m_handoverEvents.push_back(ho);
    
    // Maintain max size
    while (m_handoverEvents.size() > m_telemetryConfig.maxHandoverHistory)
    {
        m_handoverEvents.pop_front();
    }
    
    // Log event
    std::ostringstream desc;
    desc << "UE " << ueId << " handover " << sourceCellId << " → " << targetCellId
         << (success ? " ✓" : " ✗");
    LogEvent("handover", desc.str());
    
    // Trigger update if enabled
    if (m_telemetryConfig.eventTriggeredUpdates && m_telemetryEnabled)
    {
        PublishStateNow("handover");
    }
}

void
NrOutputManager::OnTrafficUpdate(uint32_t ueId)
{
    NS_LOG_FUNCTION(this << ueId);
    
    // Could be used for traffic alerts
    // For now, just rely on periodic updates
}

// ================================================================
// STATE COLLECTION
// ================================================================

NrOutputManager::SimulationState
NrOutputManager::CollectCurrentState()
{
    NS_LOG_FUNCTION(this);
    
    auto startTime = std::chrono::steady_clock::now();
    
    SimulationState state;
    
    // ===== Timestamp information =====
    state.simulationTime = Simulator::Now().GetSeconds();
    state.wallClockTime = GetCurrentTimeIso8601();
    
    auto elapsed = std::chrono::steady_clock::now() - m_wallClockStart;
    state.wallClockSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    // ===== Simulation status =====
    if (m_config != nullptr)
    {
        state.totalDuration = m_config->simDuration;
        state.progressPercent = (state.simulationTime / state.totalDuration) * 100.0;
        
        if (state.simulationTime < 0.1)
            state.status = "initializing";
        else if (state.simulationTime >= state.totalDuration - 0.1)
            state.status = "finalizing";
        else
            state.status = "running";
    }
    else
    {
        state.status = "unknown";
        state.totalDuration = 0.0;
        state.progressPercent = 0.0;
    }
    
    // ===== Topology =====
    if (m_topologyManager != nullptr)
    {
        NodeContainer ueNodes = m_topologyManager->GetUeNodes();
        NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
        
        state.ueCount = ueNodes.GetN();
        state.gnbCount = gnbNodes.GetN();
        
        // Collect UE states
        for (uint32_t i = 0; i < state.ueCount; ++i)
        {
            SimulationState::UeState ueState = CollectUeState(i);
            state.ues.push_back(ueState);
        }
        
        // Collect gNB states
        for (uint32_t i = 0; i < state.gnbCount; ++i)
        {
            SimulationState::GnbState gnbState = CollectGnbState(i);
            state.gnbs.push_back(gnbState);
        }
    }
    else
    {
        state.ueCount = 0;
        state.gnbCount = 0;
    }
    
    // ===== Aggregate statistics =====
    if (m_telemetryConfig.includeTrafficStats)
    {
        CollectAggregateStats(state);
    }
    
    // ===== Handover history =====
    if (m_telemetryConfig.includeHandovers)
    {
        CollectHandoverHistory(state);
    }
    
    // ===== Event log =====
    if (m_telemetryConfig.includeEventLog)
    {
        state.recentEvents = m_eventLog;
    }
    
    // ===== Track generation time =====
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    m_stateGenTimes.push_back(duration.count());
    
    // Keep history buffer limited
    if (m_stateGenTimes.size() > 1000)
    {
        m_stateGenTimes.erase(m_stateGenTimes.begin());
    }
    
    // Store in history if enabled
    if (m_telemetryConfig.maxHistorySize > 0)
    {
        m_stateHistory.push_back(state);
        
        while (m_stateHistory.size() > m_telemetryConfig.maxHistorySize)
        {
            m_stateHistory.pop_front();
        }
    }
    
    return state;
}

NrOutputManager::SimulationState::UeState
NrOutputManager::CollectUeState(uint32_t ueId)
{
    SimulationState::UeState ueState;
    
    // Basic info
    ueState.ueId = ueId;
    ueState.imsi = 0;  // Will be set if available
    
    // Get UE node
    NodeContainer ueNodes = m_topologyManager->GetUeNodes();
    if (ueId >= ueNodes.GetN())
    {
        NS_LOG_WARN("UE " << ueId << " out of range");
        return ueState;
    }
    
    Ptr<Node> ueNode = ueNodes.Get(ueId);
    
    // ===== IMSI =====
    // Try to get IMSI from NetworkManager if method exists
    // For now, use approximation based on typical assignment
    ueState.imsi = ueId + 7;  // TODO: Add GetUeImsi() to NetworkManager
    
    // ===== Position and Velocity =====
    if (m_telemetryConfig.includePositions)
    {
        Ptr<MobilityModel> mobility = ueNode->GetObject<MobilityModel>();
        if (mobility != nullptr)
        {
            ueState.position = mobility->GetPosition();
            
            if (m_telemetryConfig.includeVelocities)
            {
                ueState.velocity = mobility->GetVelocity();
                ueState.speed = ueState.velocity.GetLength();
            }
            else
            {
                ueState.velocity = Vector(0, 0, 0);
                ueState.speed = 0.0;
            }
            
            // Determine mobility model type
            if (DynamicCast<WaypointMobilityModel>(mobility) != nullptr)
            {
                ueState.mobilityModel = "waypoint";
                // TODO: Get current waypoint index if WaypointMobilityModel exposes it
                ueState.currentWaypoint = 0;
                ueState.totalWaypoints = 0;
            }
            else if (DynamicCast<RandomWalk2dMobilityModel>(mobility) != nullptr)
            {
                ueState.mobilityModel = "random_walk";
                ueState.currentWaypoint = 0;
                ueState.totalWaypoints = 0;
            }
            else
            {
                ueState.mobilityModel = "static";
                ueState.currentWaypoint = 0;
                ueState.totalWaypoints = 0;
            }
        }
        else
        {
            ueState.position = Vector(0, 0, 0);
            ueState.velocity = Vector(0, 0, 0);
            ueState.speed = 0.0;
            ueState.mobilityModel = "none";
            ueState.currentWaypoint = 0;
            ueState.totalWaypoints = 0;
        }
    }
    
    // ===== Network Attachment =====
    if (m_telemetryConfig.includeAttachments && m_networkManager != nullptr)
    {
        ueState.cellId = m_networkManager->GetServingGnb(ueId);
        // gnbId will be resolved below via the distance loop (closest gNB index)
        ueState.gnbId = 0;  // default; overwritten when positions are available
        
        // Calculate distance to serving gNB AND resolve gnbId
        if (m_telemetryConfig.includePositions)
        {
            NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
            
            // Find the closest gNB — this also gives us the node index,
            // which is the gnbId the dashboard needs to draw connection lines.
            double   minDist       = 1e9;
            uint32_t closestGnbIdx = 0;
            for (uint32_t g = 0; g < gnbNodes.GetN(); ++g)
            {
                Ptr<MobilityModel> gnbMob = gnbNodes.Get(g)->GetObject<MobilityModel>();
                if (gnbMob != nullptr)
                {
                    Vector gnbPos = gnbMob->GetPosition();
                    double dist   = CalculateDistance(ueState.position, gnbPos);
                    if (dist < minDist)
                    {
                        minDist       = dist;
                        closestGnbIdx = g;
                    }
                }
            }
            ueState.distanceToGnb = minDist;
            ueState.gnbId         = closestGnbIdx;
        }
        else
        {
            ueState.distanceToGnb = 0.0;
        }
    }
    else
    {
        ueState.cellId = 0;
        ueState.gnbId = 0;
        ueState.distanceToGnb = 0.0;
    }
    
    // ===== Traffic Statistics =====
    ueState.hasRadioMetrics = false;
    ueState.hasBufferMetrics = false;
    
    if (m_telemetryConfig.includeTrafficStats)
    {
        CollectUeTrafficStats(ueState);
    }
    else
    {
        // Initialize to zero
        ueState.dlThroughputMbps = 0.0;
        ueState.ulThroughputMbps = 0.0;
        ueState.dlPacketsTx = 0;
        ueState.dlPacketsRx = 0;
        ueState.ulPacketsTx = 0;
        ueState.ulPacketsRx = 0;
        ueState.dlLossPct = 0.0;
        ueState.ulLossPct = 0.0;
        ueState.avgDelayMs = 0.0;
    }
    
    // ===== Radio Metrics (if available) =====
    if (m_telemetryConfig.includeRadioMetrics)
    {
        CollectUeRadioMetrics(ueState);
    }
    
    // ===== Buffer Metrics (if available) =====
    if (m_telemetryConfig.includeBufferMetrics)
    {
        CollectUeBufferMetrics(ueState);
    }
    
    return ueState;
}

NrOutputManager::SimulationState::GnbState
NrOutputManager::CollectGnbState(uint32_t gnbId)
{
    SimulationState::GnbState gnbState;
    
    gnbState.gnbId = gnbId;
    gnbState.cellId = gnbId;  // default; overwritten below if device is available
    gnbState.attachedUeCount = 0;
    gnbState.hasSchedulerMetrics = false;
    gnbState.hasBufferMetrics = false;
    
    // Get gNB node
    NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
    if (gnbId >= gnbNodes.GetN())
    {
        NS_LOG_WARN("gNB " << gnbId << " out of range");
        return gnbState;
    }
    
    Ptr<Node> gnbNode = gnbNodes.Get(gnbId);

    // ── Pull the real EPC-assigned cell_id off the net-device ──
    // EPC numbers cells starting at 1, so for gnbId==0 this will
    // typically return 1.  Without this the attachment loop below
    // compares GetServingGnb() (==1) against 0 and never matches.
    
    // Access the gNB device from NetworkManager's container (not from node)
    // This is the correct pattern - see NrNetworkManager line 351 for reference
    Ptr<NrGnbNetDevice> gnbNetDev = nullptr;
    if (m_networkManager != nullptr)
    {
        NetDeviceContainer gnbDevices = m_networkManager->GetGnbDevices();
        if (gnbId < gnbDevices.GetN())
        {
            Ptr<NetDevice> dev = gnbDevices.Get(gnbId);
            gnbNetDev = DynamicCast<NrGnbNetDevice>(dev);
        }
    }
    
    if (gnbNetDev != nullptr)
    {
        gnbState.cellId = gnbNetDev->GetCellId();
        NS_LOG_DEBUG("gNB " << gnbId << " real cellId = " << gnbState.cellId);
        
        // ── Get Scheduler Type ──
        // Query the scheduler from BWP 0 (primary bandwidth part).
        // If your setup uses multiple BWPs, you might need to iterate all.
        try
        {
            Ptr<NrMacScheduler> scheduler = gnbNetDev->GetScheduler(0);
            if (scheduler != nullptr)
            {
                gnbState.scheduler_type = scheduler->GetInstanceTypeId().GetName();
                NS_LOG_DEBUG("gNB " << gnbId << " scheduler: " << gnbState.scheduler_type);
            }
            else
            {
                gnbState.scheduler_type = "unknown";
            }
        }
        catch (...)
        {
            gnbState.scheduler_type = "unavailable";
            NS_LOG_DEBUG("gNB " << gnbId << " scheduler access failed");
        }
    }
    else
    {
        NS_LOG_WARN("gNB " << gnbId << " has no NrGnbNetDevice — cellId defaulting to gnbId");
        gnbState.scheduler_type = "no_device";
    }
    
    // Position
    Ptr<MobilityModel> mobility = gnbNode->GetObject<MobilityModel>();
    if (mobility != nullptr)
    {
        gnbState.position = mobility->GetPosition();
    }
    else
    {
        gnbState.position = Vector(0, 0, 0);
    }
    
    // Count attached UEs
    if (m_networkManager != nullptr)
    {
        NodeContainer ueNodes = m_topologyManager->GetUeNodes();
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            uint16_t servingCell = m_networkManager->GetServingGnb(i);
            if (servingCell == gnbState.cellId)
            {
                gnbState.attachedUeCount++;
                gnbState.attachedUeIds.push_back(i);
            }
        }
    }
    
    return gnbState;
}

//===============================================================
// TRAFFIC STATISTICS COLLECTION
//===============================================================
void
NrOutputManager::CollectUeTrafficStats(SimulationState::UeState& ueState)
{
    if (m_trafficManager != nullptr)
    {
        // Get real-time metrics from TrafficManager
        PerUeMetrics metrics = m_trafficManager->GetUeMetrics(ueState.ueId);
        
        // Downlink
        ueState.dlThroughputMbps = metrics.dlThroughputMbps;
        ueState.dlPacketsTx = metrics.dlTxPackets;
        ueState.dlPacketsRx = metrics.dlRxPackets;
        ueState.dlLossPct = metrics.dlPacketLossRate * 100.0;  // Convert to percentage
        ueState.avgDelayMs = metrics.dlAvgDelayMs;
        
        // Uplink  
        ueState.ulThroughputMbps = metrics.ulThroughputMbps;
        ueState.ulPacketsTx = metrics.ulTxPackets;
        ueState.ulPacketsRx = metrics.ulRxPackets;
        ueState.ulLossPct = metrics.ulPacketLossRate * 100.0;  // Convert to percentage
    }
    else
    {
        // No traffic manager - all zeros
        ueState.dlThroughputMbps = 0.0;
        ueState.ulThroughputMbps = 0.0;
        ueState.dlPacketsTx = 0;
        ueState.dlPacketsRx = 0;
        ueState.ulPacketsTx = 0;
        ueState.ulPacketsRx = 0;
        ueState.dlLossPct = 0.0;
        ueState.ulLossPct = 0.0;
        ueState.avgDelayMs = 0.0;
    }
}


void
NrOutputManager::CollectAggregateStats(SimulationState& state)
{
    if (m_trafficManager != nullptr)
    {
        AggregateMetrics agg = m_trafficManager->GetAggregateMetrics();
        
        state.totalDlThroughputMbps = agg.totalDlThroughputMbps;
        state.totalUlThroughputMbps = agg.totalUlThroughputMbps;
        state.avgPacketLossPct = agg.overallPacketLossRate * 100.0;  // Convert to percentage
    }
    else
    {
        // Fallback to summing from UE states
        state.totalDlThroughputMbps = 0.0;
        state.totalUlThroughputMbps = 0.0;
        double totalLoss = 0.0;
        uint32_t ueCount = 0;
        
        for (const auto& ue : state.ues)
        {
            state.totalDlThroughputMbps += ue.dlThroughputMbps;
            state.totalUlThroughputMbps += ue.ulThroughputMbps;
            totalLoss += ue.dlLossPct;
            ueCount++;
        }
        
        if (ueCount > 0)
            state.avgPacketLossPct = totalLoss / ueCount;
        else
            state.avgPacketLossPct = 0.0;
    }
}




void
NrOutputManager::CollectUeRadioMetrics(SimulationState::UeState& ueState)
{
    // Default to unavailable
    ueState.hasRadioMetrics = false;
    ueState.rsrpDbm = 0.0;
    ueState.sinrDb = 0.0;
    ueState.cqi = 0;
    ueState.mcs = 0;
    
    std::cout << "DEBUG: CollectUeRadioMetrics for UE " << ueState.ueId << std::endl;
    
    if (m_networkManager == nullptr)
    {
        std::cout << "  FAIL: m_networkManager is nullptr" << std::endl;
        return;
    }
    
    std::cout << "  NetworkManager OK" << std::endl;
    
    // Access the UE device from NetworkManager's container
    NetDeviceContainer ueDevices = m_networkManager->GetUeDevices();
    std::cout << "  GetUeDevices returned container with " << ueDevices.GetN() << " devices" << std::endl;
    
    if (ueState.ueId >= ueDevices.GetN())
    {
        std::cout << "  FAIL: UE " << ueState.ueId << " >= " << ueDevices.GetN() << std::endl;
        return;
    }
    
    Ptr<NetDevice> dev = ueDevices.Get(ueState.ueId);
    std::cout << "  Got NetDevice pointer: " << (dev ? "valid" : "nullptr") << std::endl;
    
    Ptr<NrUeNetDevice> ueNetDev = DynamicCast<NrUeNetDevice>(dev);
    std::cout << "  DynamicCast result: " << (ueNetDev ? "valid" : "nullptr") << std::endl;
    
    if (ueNetDev == nullptr)
    {
        std::cout << "  FAIL: DynamicCast returned nullptr" << std::endl;
        // Try to see what type it actually is
        std::cout << "  Device TypeId: " << dev->GetInstanceTypeId().GetName() << std::endl;
        return;
    }
    
    // Get the PHY from the primary BWP
    Ptr<NrUePhy> uePhy = nullptr;
    try
    {
        std::cout << "  Attempting GetPhy(0)..." << std::endl;
        uePhy = ueNetDev->GetPhy(0);
        std::cout << "  GetPhy(0) returned: " << (uePhy ? "valid" : "nullptr") << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "  EXCEPTION in GetPhy(0): " << e.what() << std::endl;
        return;
    }
    catch (...)
    {
        std::cout << "  UNKNOWN EXCEPTION in GetPhy(0)" << std::endl;
        return;
    }
    
    if (uePhy == nullptr)
    {
        std::cout << "  FAIL: uePhy is nullptr" << std::endl;
        return;
    }
    
    // Extract RSRP
    ueState.hasRadioMetrics = true;
    ueState.rsrpDbm = uePhy->GetRsrp();
    
    std::cout << "  SUCCESS: RSRP = " << ueState.rsrpDbm << " dBm" << std::endl;
}

void
NrOutputManager::CollectUeBufferMetrics(SimulationState::UeState& ueState)
{
    // Buffer metrics require MAC/RLC tracing
    // This is Phase 3 - for now, mark as unavailable
    
    ueState.hasBufferMetrics = false;
    ueState.ulBufferBytes = 0;
    ueState.dlBufferBytes = 0;
}


void
NrOutputManager::CollectHandoverHistory(SimulationState& state)
{
    if (m_networkManager != nullptr)
    {
        state.totalHandovers = m_networkManager->GetTotalHandovers();
    }
    else
    {
        state.totalHandovers = 0;
    }
    
    // Copy recent handover events
    state.recentHandovers = m_handoverEvents;
}

// ================================================================
// JSON FORMATTING
// ================================================================

std::string
NrOutputManager::StateToJson(const SimulationState& state, bool prettyPrint)
{
    NS_LOG_FUNCTION(this << prettyPrint);
    
    using json = nlohmann::json;
    json j;
    
    // ===== Metadata =====
    j["version"] = "1.0";
    
    // ===== Timestamp =====
    j["timestamp"]["simulation_time"] = state.simulationTime;
    j["timestamp"]["wall_clock_time"] = state.wallClockTime;
    j["timestamp"]["wall_clock_seconds"] = state.wallClockSeconds;
    
    // ===== Simulation Status =====
    j["simulation"]["status"] = state.status;
    j["simulation"]["progress_percent"] = state.progressPercent;
    j["simulation"]["total_duration"] = state.totalDuration;
    
    // ===== Configuration =====
    j["config"]["gnb_count"] = state.gnbCount;
    j["config"]["ue_count"] = state.ueCount;
    
    if (m_config != nullptr)
    {
        j["config"]["bandwidth_mhz"] = m_config->channel.bandwidth / 1e6;
        j["config"]["frequency_ghz"] = m_config->channel.frequency / 1e9;
        j["config"]["area_size"]     = m_config->topology.areaSize;
    }
    
    // ===== UE Topology =====
    j["topology"]["ues"] = json::array();
    
    for (const auto& ue : state.ues)
    {
        json jUe;
        jUe["id"] = ue.ueId;
        jUe["imsi"] = ue.imsi;
        
        if (m_telemetryConfig.includePositions)
        {
            jUe["position"]["x"] = ue.position.x;
            jUe["position"]["y"] = ue.position.y;
            jUe["position"]["z"] = ue.position.z;
            
            if (m_telemetryConfig.includeVelocities)
            {
                jUe["velocity"]["x"] = ue.velocity.x;
                jUe["velocity"]["y"] = ue.velocity.y;
                jUe["velocity"]["z"] = ue.velocity.z;
                jUe["speed"] = ue.speed;
            }
            
            jUe["mobility_model"] = ue.mobilityModel;
            
            if (ue.mobilityModel == "waypoint")
            {
                jUe["waypoint_progress"]["current"] = ue.currentWaypoint;
                jUe["waypoint_progress"]["total"] = ue.totalWaypoints;
            }
        }
        
        if (m_telemetryConfig.includeAttachments)
        {
            jUe["network"]["cell_id"] = ue.cellId;
            jUe["network"]["gnb_id"] = ue.gnbId;
            jUe["network"]["distance_to_gnb"] = ue.distanceToGnb;
        }
        
        if (m_telemetryConfig.includeRadioMetrics && ue.hasRadioMetrics)
        {
            jUe["radio"]["available"] = true;
            jUe["radio"]["rsrp_dbm"] = ue.rsrpDbm;
            jUe["radio"]["sinr_db"] = ue.sinrDb;
            jUe["radio"]["cqi"] = ue.cqi;
            jUe["radio"]["mcs"] = ue.mcs;
        }
        else
        {
            jUe["radio"]["available"] = false;
        }
        
        if (m_telemetryConfig.includeTrafficStats)
        {
            jUe["traffic"]["dl"]["throughput_mbps"] = ue.dlThroughputMbps;
            jUe["traffic"]["dl"]["packets_tx"] = ue.dlPacketsTx;
            jUe["traffic"]["dl"]["packets_rx"] = ue.dlPacketsRx;
            jUe["traffic"]["dl"]["loss_percent"] = ue.dlLossPct;
            jUe["traffic"]["dl"]["avg_delay_ms"] = ue.avgDelayMs;
            
            jUe["traffic"]["ul"]["throughput_mbps"] = ue.ulThroughputMbps;
            jUe["traffic"]["ul"]["packets_tx"] = ue.ulPacketsTx;
            jUe["traffic"]["ul"]["packets_rx"] = ue.ulPacketsRx;
            jUe["traffic"]["ul"]["loss_percent"] = ue.ulLossPct;
        }
        
        if (m_telemetryConfig.includeBufferMetrics && ue.hasBufferMetrics)
        {
            jUe["buffers"]["available"] = true;
            jUe["buffers"]["ul_bytes"] = ue.ulBufferBytes;
            jUe["buffers"]["dl_bytes"] = ue.dlBufferBytes;
        }
        else
        {
            jUe["buffers"]["available"] = false;
        }
        
        j["topology"]["ues"].push_back(jUe);
    }
    
    // ===== gNB Topology =====
    j["topology"]["gnbs"] = json::array();
    
    for (const auto& gnb : state.gnbs)
    {
        json jGnb;
        jGnb["id"] = gnb.gnbId;
        jGnb["cell_id"] = gnb.cellId;
        
        jGnb["position"]["x"] = gnb.position.x;
        jGnb["position"]["y"] = gnb.position.y;
        jGnb["position"]["z"] = gnb.position.z;
        
        jGnb["attached_ues"]["count"] = gnb.attachedUeCount;
        jGnb["attached_ues"]["ue_ids"] = gnb.attachedUeIds;
        
        // Scheduler type (always available if we have a net device)
        jGnb["scheduler"]["type"] = gnb.scheduler_type;
        
        if (m_telemetryConfig.includeSchedulerMetrics && gnb.hasSchedulerMetrics)
        {
            jGnb["scheduler"]["available"] = true;
            jGnb["scheduler"]["utilization_percent"] = gnb.resourceUtilizationPct;
            jGnb["scheduler"]["allocated_rbs"] = gnb.allocatedRbs;
            jGnb["scheduler"]["total_rbs"] = gnb.totalRbs;
        }
        else
        {
            jGnb["scheduler"]["available"] = false;
        }
        
        if (m_telemetryConfig.includeBufferMetrics && gnb.hasBufferMetrics)
        {
            jGnb["buffers"]["available"] = true;
            jGnb["buffers"]["dl_queue_bytes"] = gnb.dlQueueBytes;
            jGnb["buffers"]["dl_queue_packets"] = gnb.dlQueuePackets;
        }
        else
        {
            jGnb["buffers"]["available"] = false;
        }
        
        j["topology"]["gnbs"].push_back(jGnb);
    }
    
    // ===== Traffic Summary =====
    if (m_telemetryConfig.includeTrafficStats)
    {
        j["traffic_summary"]["total_dl_throughput_mbps"] = state.totalDlThroughputMbps;
        j["traffic_summary"]["total_ul_throughput_mbps"] = state.totalUlThroughputMbps;
        j["traffic_summary"]["avg_packet_loss_percent"] = state.avgPacketLossPct;
    }
    
    // ===== Handovers =====
    if (m_telemetryConfig.includeHandovers)
    {
        j["handovers"]["total_count"] = state.totalHandovers;
        j["handovers"]["recent_events"] = json::array();
        
        for (const auto& ho : state.recentHandovers)
        {
            json jHo;
            jHo["timestamp"] = ho.timestamp;
            jHo["ue_id"] = ho.ueId;
            jHo["source_cell_id"] = ho.sourceCellId;
            jHo["target_cell_id"] = ho.targetCellId;
            jHo["success"] = ho.success;
            jHo["reason"] = ho.reason;
            
            j["handovers"]["recent_events"].push_back(jHo);
        }
    }
    
    // ===== Events =====
    if (m_telemetryConfig.includeEventLog && !state.recentEvents.empty())
    {
        j["events"]["recent"] = json::array();
        
        for (const auto& evt : state.recentEvents)
        {
            json jEvt;
            jEvt["timestamp"] = evt.timestamp;
            jEvt["type"] = evt.type;
            jEvt["description"] = evt.description;
            jEvt["details"] = evt.details;
            
            j["events"]["recent"].push_back(jEvt);
        }
    }
    
    // ===== Convert to string =====
    std::string jsonStr;
    if (prettyPrint)
    {
        jsonStr = j.dump(2);  // Indent with 2 spaces
    }
    else
    {
        jsonStr = j.dump();  // Compact
    }
    
    // Track size
    m_jsonSizes.push_back(jsonStr.size());
    if (m_jsonSizes.size() > 1000)
    {
        m_jsonSizes.erase(m_jsonSizes.begin());
    }
    
    return jsonStr;
}

std::vector<std::string>
NrOutputManager::StateToCsv(const SimulationState& state)
{
    std::vector<std::string> rows;
    
    // Header
    rows.push_back("time,ue_id,pos_x,pos_y,cell_id,dl_mbps,ul_mbps,dl_loss,ul_loss");
    
    // Data rows
    for (const auto& ue : state.ues)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << state.simulationTime << ","
            << ue.ueId << ","
            << ue.position.x << ","
            << ue.position.y << ","
            << ue.cellId << ","
            << ue.dlThroughputMbps << ","
            << ue.ulThroughputMbps << ","
            << ue.dlLossPct << ","
            << ue.ulLossPct;
        
        rows.push_back(oss.str());
    }
    
    return rows;
}

std::string
NrOutputManager::GenerateSummaryReport(const SimulationState& state)
{
    std::ostringstream report;
    
    report << "========================================\n";
    report << "NR SIMULATION FINAL SUMMARY\n";
    report << "========================================\n\n";
    
    report << "Simulation completed at t=" << state.simulationTime << "s\n";
    report << "Status: " << state.status << "\n\n";
    
    report << "Network Topology:\n";
    report << "  gNBs: " << state.gnbCount << "\n";
    report << "  UEs: " << state.ueCount << "\n\n";
    
    if (m_telemetryConfig.includeTrafficStats)
    {
        report << "Traffic Summary:\n";
        report << "  Total DL Throughput: " << state.totalDlThroughputMbps << " Mbps\n";
        report << "  Total UL Throughput: " << state.totalUlThroughputMbps << " Mbps\n";
        report << "  Avg Packet Loss: " << state.avgPacketLossPct << "%\n\n";
    }
    
    if (m_telemetryConfig.includeHandovers)
    {
        report << "Mobility:\n";
        report << "  Total Handovers: " << state.totalHandovers << "\n\n";
    }
    
    report << "Per-UE Statistics:\n";
    for (const auto& ue : state.ues)
    {
        report << "  UE " << ue.ueId << ":\n";
        report << "    Position: (" << ue.position.x << ", " << ue.position.y << ")\n";
        report << "    Cell: " << ue.cellId << "\n";
        
        if (m_telemetryConfig.includeTrafficStats)
        {
            report << "    DL: " << ue.dlThroughputMbps << " Mbps, " 
                   << ue.dlLossPct << "% loss\n";
            report << "    UL: " << ue.ulThroughputMbps << " Mbps, " 
                   << ue.ulLossPct << "% loss\n";
        }
    }
    
    report << "\n========================================\n";
    
    return report.str();
}

// ================================================================
// PUBLISHING
// ================================================================

void
NrOutputManager::ConfigurePublishing(PublishMethod method,
                                    const std::string& host,
                                    uint16_t port,
                                    const std::string& filepath)
{
    NS_LOG_FUNCTION(this << (int)method << host << port << filepath);
    
    m_publishMethod = method;
    m_publishHost = host;
    m_publishPort = port;
    m_publishFilepath = filepath;
    
    NS_LOG_INFO("Publishing configured: method=" << (int)method);
}

void
NrOutputManager::PeriodicPublish()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_telemetryEnabled)
        return;
    
    // Collect current state
    SimulationState state = CollectCurrentState();
    
    // Publish
    PublishState(state, "periodic");
    
    // Schedule next update
    ScheduleNextUpdate();
}

void
NrOutputManager::PublishState(const SimulationState& state, const std::string& trigger)
{
    NS_LOG_FUNCTION(this << trigger);
    
    if (m_publishMethod == PUBLISH_DISABLED)
        return;
    
    // Convert to JSON
    std::string json = StateToJson(state, false);
    
    // Publish via configured method
    bool success = false;
    
    switch (m_publishMethod)
    {
        case PUBLISH_FILE:
            success = PublishToFile(json, m_publishFilepath);
            break;
            
        case PUBLISH_UDP:
            success = PublishViaUdp(json);
            break;
            
        case PUBLISH_TCP:
            success = PublishViaTcp(json);
            break;
            
        case PUBLISH_PIPE:
            success = PublishToPipe(json);
            break;
            
        default:
            return;
    }
    
    if (success)
    {
        m_publishedStateCount++;
        m_lastPublishTime = Simulator::Now();
        
        NS_LOG_DEBUG("Published state #" << m_publishedStateCount 
                    << " (" << json.size() << " bytes) trigger=" << trigger);
    }
    else
    {
        m_failedPublishCount++;
        NS_LOG_WARN("Failed to publish state (failure #" << m_failedPublishCount << ")");
    }
}

bool
NrOutputManager::PublishToFile(const std::string& json, const std::string& filepath)
{
    NS_LOG_FUNCTION(this << filepath);
    
    try
    {
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            NS_LOG_ERROR("Failed to open file: " << filepath);
            return false;
        }
        
        file << json;
        file.close();
        
        return true;
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Exception writing to file: " << e.what());
        return false;
    }
}

bool
NrOutputManager::PublishViaUdp(const std::string& json)
{
    NS_LOG_FUNCTION(this);
    
    if (m_config->debug.enableDebugLogs)
    {
        std::cout << "[DEBUG] PublishViaUdp called, size=" << json.size() << " bytes" << std::endl;
    }
    
    // Create socket if not exists
    if (m_udpSocket < 0)
    {
        m_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_udpSocket < 0)
        {
            NS_LOG_ERROR("Failed to create UDP socket: " << strerror(errno));
            std::cerr << "[ERROR] Failed to create UDP socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        NS_LOG_INFO("UDP socket created");
        if (m_config->debug.enableDebugLogs)
        {
            std::cout << "[DEBUG] UDP socket created" << std::endl;
        }
    }
    
    // Convert "localhost" to "127.0.0.1" to avoid DNS issues
    std::string targetHost = m_publishHost;
    if (targetHost == "localhost")
    {
        targetHost = "127.0.0.1";
    }
    
    // Setup address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_publishPort);
    
    if (inet_pton(AF_INET, targetHost.c_str(), &addr.sin_addr) <= 0)
    {
        NS_LOG_ERROR("Invalid address: " << targetHost);
        std::cerr << "[ERROR] Invalid address: " << targetHost << std::endl;
        return false;
    }
    
    if (m_config->debug.enableDebugLogs)
    {
        std::cout << "[DEBUG] Sending UDP packet to " << targetHost 
                  << ":" << m_publishPort << std::endl;
    }
    
    // Send data
    ssize_t sent = sendto(m_udpSocket, json.c_str(), json.size(), 0,
                         (struct sockaddr*)&addr, sizeof(addr));
    
    if (sent < 0)
    {
        NS_LOG_ERROR("Failed to send UDP packet: " << strerror(errno));
        std::cerr << "[ERROR] sendto failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (m_config->debug.enableDebugLogs)
    {
        std::cout << "[DEBUG] UDP packet sent, bytes=" << sent << std::endl;
    }
    
    if ((size_t)sent != json.size())
    {
        NS_LOG_WARN("Partial UDP send: " << sent << " / " << json.size() << " bytes");
        std::cerr << "[WARNING] Partial send" << std::endl;
        return false;
    }
    
    return true;
}

bool
NrOutputManager::PublishViaTcp(const std::string& json)
{
    NS_LOG_FUNCTION(this);
    
    // TCP implementation - for Phase 2
    // Would need connection management, reconnection logic, etc.
    
    NS_LOG_WARN("TCP publishing not yet implemented");
    return false;
}

bool
NrOutputManager::PublishToPipe(const std::string& json)
{
    NS_LOG_FUNCTION(this);
    
    // Named pipe implementation - for Phase 2
    // Would use open() with O_WRONLY | O_NONBLOCK
    
    NS_LOG_WARN("Pipe publishing not yet implemented");
    return false;
}

// ================================================================
// UTILITIES
// ================================================================

double
NrOutputManager::CalculateDistance(const Vector& pos1, const Vector& pos2)
{
    double dx = pos1.x - pos2.x;
    double dy = pos1.y - pos2.y;
    double dz = pos1.z - pos2.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

std::string
NrOutputManager::GetCurrentTimeIso8601()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

void
NrOutputManager::LogEvent(const std::string& type, const std::string& description)
{
    SimulationState::SimulationEvent evt;
    evt.timestamp = Simulator::Now().GetSeconds();
    evt.type = type;
    evt.description = description;
    
    m_eventLog.push_back(evt);
    
    // Maintain max size
    while (m_eventLog.size() > m_telemetryConfig.maxEventHistory)
    {
        m_eventLog.pop_front();
    }
}

void
NrOutputManager::ScheduleNextUpdate()
{
    if (!m_telemetryEnabled)
        return;
    
    m_publishEvent = Simulator::Schedule(
        m_publishInterval,
        &NrOutputManager::PeriodicPublish,
        this
    );
}

// ================================================================
// STATE HISTORY
// ================================================================

std::vector<NrOutputManager::SimulationState>
NrOutputManager::GetStateHistory(uint32_t count)
{
    if (count == 0 || count >= m_stateHistory.size())
    {
        // Return all history
        return std::vector<SimulationState>(m_stateHistory.begin(), m_stateHistory.end());
    }
    else
    {
        // Return last N states
        auto start = m_stateHistory.end() - count;
        return std::vector<SimulationState>(start, m_stateHistory.end());
    }
}

// ================================================================
// STATISTICS
// ================================================================

uint64_t
NrOutputManager::GetPublishedStateCount() const
{
    return m_publishedStateCount;
}

uint64_t
NrOutputManager::GetFailedPublishCount() const
{
    return m_failedPublishCount;
}

double
NrOutputManager::GetAvgStateGenerationTimeMs() const
{
    if (m_stateGenTimes.empty())
        return 0.0;
    
    double sum = 0.0;
    for (double t : m_stateGenTimes)
    {
        sum += t;
    }
    
    return sum / m_stateGenTimes.size();
}

uint64_t
NrOutputManager::GetAvgJsonSizeBytes() const
{
    if (m_jsonSizes.empty())
        return 0;
    
    uint64_t sum = 0;
    for (uint64_t s : m_jsonSizes)
    {
        sum += s;
    }
    
    return sum / m_jsonSizes.size();
}

void
NrOutputManager::PrintTelemetryStats() const
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Telemetry Statistics" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "Published states: " << m_publishedStateCount << std::endl;
    std::cout << "Failed publishes: " << m_failedPublishCount << std::endl;
    
    if (m_publishedStateCount > 0)
    {
        double successRate = 100.0 * m_publishedStateCount / 
                            (m_publishedStateCount + m_failedPublishCount);
        std::cout << "Success rate: " << successRate << "%" << std::endl;
    }
    
    std::cout << "Avg generation time: " << GetAvgStateGenerationTimeMs() << " ms" << std::endl;
    std::cout << "Avg JSON size: " << GetAvgJsonSizeBytes() << " bytes" << std::endl;
    
    std::cout << "State history: " << m_stateHistory.size() << " snapshots" << std::endl;
    std::cout << "Handover events: " << m_handoverEvents.size() << " events" << std::endl;
    std::cout << "Event log: " << m_eventLog.size() << " events" << std::endl;
    
    std::cout << "========================================\n" << std::endl;
}

} // namespace ns3