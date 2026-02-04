/*
 * Copyright (c) 2026 ARTPARK
 *
 * NR Traffic Manager - Enhanced with Integrated FlowMonitor Metrics
 * Based on cttc-3gpp-indoor-calibration.cc FlowMonitor usage
 */

#ifndef NR_TRAFFIC_MANAGER_H
#define NR_TRAFFIC_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include "nr-network-manager.h"

#include <map>
#include <vector>

namespace ns3
{

class NrSimConfig;

/**
 * \brief Per-UE metrics structure
 */
struct PerUeMetrics
{
    uint32_t ueId;                    ///< UE index (0-based)
    
    // Downlink metrics
    double dlThroughputMbps;          ///< Downlink throughput (Mbps)
    double dlAvgDelayMs;              ///< Downlink average delay (ms)
    double dlJitterMs;                ///< Downlink jitter (ms)
    double dlPacketLossRate;          ///< Downlink packet loss rate (0.0-1.0)
    uint64_t dlTxPackets;             ///< Downlink packets transmitted
    uint64_t dlRxPackets;             ///< Downlink packets received
    uint64_t dlLostPackets;           ///< Downlink packets lost
    uint64_t dlTxBytes;               ///< Downlink bytes transmitted
    uint64_t dlRxBytes;               ///< Downlink bytes received
    
    // Uplink metrics
    double ulThroughputMbps;          ///< Uplink throughput (Mbps)
    double ulAvgDelayMs;              ///< Uplink average delay (ms)
    double ulJitterMs;                ///< Uplink jitter (ms)
    double ulPacketLossRate;          ///< Uplink packet loss rate (0.0-1.0)
    uint64_t ulTxPackets;             ///< Uplink packets transmitted
    uint64_t ulRxPackets;             ///< Uplink packets received
    uint64_t ulLostPackets;           ///< Uplink packets lost
    uint64_t ulTxBytes;               ///< Uplink bytes transmitted
    uint64_t ulRxBytes;               ///< Uplink bytes received
    
    /** Constructor */
    PerUeMetrics()
        : ueId(0),
          dlThroughputMbps(0.0), dlAvgDelayMs(0.0), dlJitterMs(0.0), dlPacketLossRate(0.0),
          dlTxPackets(0), dlRxPackets(0), dlLostPackets(0), dlTxBytes(0), dlRxBytes(0),
          ulThroughputMbps(0.0), ulAvgDelayMs(0.0), ulJitterMs(0.0), ulPacketLossRate(0.0),
          ulTxPackets(0), ulRxPackets(0), ulLostPackets(0), ulTxBytes(0), ulRxBytes(0)
    {
    }
};

/**
 * \brief Aggregate (system-wide) metrics structure
 */
struct AggregateMetrics
{
    double totalDlThroughputMbps;     ///< Total downlink throughput (Mbps)
    double totalUlThroughputMbps;     ///< Total uplink throughput (Mbps)
    double avgDlThroughputMbps;       ///< Average DL throughput per UE (Mbps)
    double avgUlThroughputMbps;       ///< Average UL throughput per UE (Mbps)
    double avgSystemDelayMs;          ///< Average system delay (ms)
    
    uint64_t totalPacketsSent;        ///< Total packets sent
    uint64_t totalPacketsReceived;    ///< Total packets received
    uint64_t totalPacketsLost;        ///< Total packets lost
    double overallPacketLossRate;     ///< Overall packet loss rate
    
    uint32_t numUes;                  ///< Number of UEs
    
    /** Constructor */
    AggregateMetrics()
        : totalDlThroughputMbps(0.0), totalUlThroughputMbps(0.0),
          avgDlThroughputMbps(0.0), avgUlThroughputMbps(0.0),
          avgSystemDelayMs(0.0),
          totalPacketsSent(0), totalPacketsReceived(0), totalPacketsLost(0),
          overallPacketLossRate(0.0), numUes(0)
    {
    }
};

/**
 * @brief Manager for traffic generation and metrics collection
 * 
 * Integrates FlowMonitor for both real-time and final metrics collection.
 */
class NrTrafficManager : public Object
{
public:
    static TypeId GetTypeId();

    NrTrafficManager();
    ~NrTrafficManager() override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    void SetConfig(const Ptr<NrSimConfig>& config);
    void SetNetworkManager(Ptr<NrNetworkManager> netMgr);

    // ========================================================================
    // TRAFFIC INSTALLATION
    // ========================================================================
    
    /**
     * @brief Install traffic applications and enable FlowMonitor
     * 
     * This method:
     * - Creates remote host and connects to PGW
     * - Installs DL/UL traffic applications
     * - Enables FlowMonitor on endpoint nodes
     * - Optionally schedules real-time monitoring
     */
    void InstallTraffic(const NodeContainer& gnbNodes, const NodeContainer& ueNodes);

    // ========================================================================
    // REAL-TIME MONITORING
    // ========================================================================
    
    /**
     * @brief Enable real-time throughput monitoring
     * @param interval Monitoring interval in seconds (default: 0.1)
     * 
     * When enabled, prints instantaneous throughput every 'interval' seconds
     * during simulation.
     */
    void EnableRealTimeMonitoring(double interval = 0.1);
    
    /**
     * @brief Disable real-time monitoring
     */
    void DisableRealTimeMonitoring();

    // ========================================================================
    // METRICS COLLECTION
    // ========================================================================
    
    /**
     * @brief Collect final metrics after simulation
     * 
     * Call after Simulator::Run() completes.
     * Processes FlowMonitor statistics and computes aggregates.
     */
    void CollectMetrics();

    void CollectPacketSinkStats();
    
    /**
     * @brief Print metrics summary to console
     */
    void PrintMetricsSummary() const;

    // ========================================================================
    // GETTERS
    // ========================================================================
    
    PerUeMetrics GetUeMetrics(uint32_t ueId) const;
    std::map<uint32_t, PerUeMetrics> GetAllUeMetrics() const;
    AggregateMetrics GetAggregateMetrics() const;
    
    ApplicationContainer GetDlClientApps() const;
    ApplicationContainer GetDlServerApps() const;
    ApplicationContainer GetUlClientApps() const;
    ApplicationContainer GetUlServerApps() const;
    ApplicationContainer GetServerApps() const;
    ApplicationContainer GetClientApps() const;
    
    bool IsInstalled() const;
    bool IsCollected() const;

protected:
    void DoDispose() override;

private:
    // ========================================================================
    // TRAFFIC INSTALLATION HELPERS
    // ========================================================================
    
    Ptr<Node> CreateRemoteHost();
    void InstallDownlinkTraffic(Ptr<Node> remoteHost, const NodeContainer& ueNodes);
    void InstallUplinkTraffic(Ptr<Node> remoteHost, const NodeContainer& ueNodes);
    void EnableFlowMonitor(const NodeContainer& gnbNodes, const NodeContainer& ueNodes);

    // ========================================================================
    // MONITORING HELPERS
    // ========================================================================
    
    /**
     * @brief Periodic callback for real-time monitoring
     */
    void MonitorFlows();
    
    /**
     * @brief Process FlowMonitor statistics
     */
    void ProcessFlowMonitorStats();
    
    bool IsDownlinkFlow(FlowId flowId) const;
    uint32_t GetUeIdFromFlow(FlowId flowId) const;
    void ComputeAggregateMetrics();
    void InitializeUeMetrics(uint32_t numUes);

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // Configuration
    Ptr<NrSimConfig> m_config;
    Ptr<NrNetworkManager> m_networkManager;
    
    // Applications
    ApplicationContainer m_dlServerApps;
    ApplicationContainer m_dlClientApps;
    ApplicationContainer m_ulServerApps;
    ApplicationContainer m_ulClientApps;
    ApplicationContainer m_serverApps;
    ApplicationContainer m_clientApps;
    
    // // FlowMonitor
    // Ptr<FlowMonitor> m_flowMonitor;
    // Ptr<Ipv4FlowClassifier> m_flowClassifier;
    
    // Real-time monitoring
    bool m_enableRealTimeMonitoring;
    double m_monitoringInterval;
    EventId m_monitoringEvent;

    Time m_lastSampleTime;
    std::vector<uint64_t> m_lastDlRxBytes;
    std::vector<uint64_t> m_lastUlRxBytes;
    
    // Metrics storage
    std::map<uint32_t, PerUeMetrics> m_ueMetrics;
    std::map<uint32_t, PerUeMetrics> m_previousMetrics;  // For calculating instantaneous rates
    AggregateMetrics m_aggregateMetrics;
    
    // State
    bool m_installed;
    bool m_metricsCollected;
    
    // Nodes
    Ptr<Node> m_remoteHost;
    NodeContainer m_ueNodes;
    NodeContainer m_gnbNodes;
    
    // Timing
    double m_trafficStartTime;
    double m_trafficDuration;
};

} // namespace ns3

#endif // NR_TRAFFIC_MANAGER_H