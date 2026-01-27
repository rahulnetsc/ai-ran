/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * NR Traffic Manager - Enhanced with Integrated Metrics Collection
 */

#ifndef NR_TRAFFIC_MANAGER_H
#define NR_TRAFFIC_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "nr-network-manager.h"

#include <map>

namespace ns3
{

class NrSimConfig;

/**
 * \brief Per-UE metrics structure
 */
struct PerUeMetrics
{
    uint32_t ueId;                    ///< UE index (0-based)
    
    // Application Layer Metrics (from FlowMonitor)
    double dlThroughputMbps;          ///< Downlink throughput (Mbps)
    double ulThroughputMbps;          ///< Uplink throughput (Mbps)
    double dlAvgDelayMs;              ///< Downlink average delay (ms)
    double ulAvgDelayMs;              ///< Uplink average delay (ms)
    double dlJitterMs;                ///< Downlink jitter (ms)
    double ulJitterMs;                ///< Uplink jitter (ms)
    double dlPacketLossRate;          ///< Downlink packet loss rate (0.0-1.0)
    double ulPacketLossRate;          ///< Uplink packet loss rate (0.0-1.0)
    
    uint64_t dlTxPackets;             ///< Downlink packets transmitted
    uint64_t dlRxPackets;             ///< Downlink packets received
    uint64_t dlLostPackets;           ///< Downlink packets lost
    uint64_t ulTxPackets;             ///< Uplink packets transmitted
    uint64_t ulRxPackets;             ///< Uplink packets received
    uint64_t ulLostPackets;           ///< Uplink packets lost
    
    /** Constructor - initialize all values to zero */
    PerUeMetrics()
        : ueId(0),
          dlThroughputMbps(0.0),
          ulThroughputMbps(0.0),
          dlAvgDelayMs(0.0),
          ulAvgDelayMs(0.0),
          dlJitterMs(0.0),
          ulJitterMs(0.0),
          dlPacketLossRate(0.0),
          ulPacketLossRate(0.0),
          dlTxPackets(0),
          dlRxPackets(0),
          dlLostPackets(0),
          ulTxPackets(0),
          ulRxPackets(0),
          ulLostPackets(0)
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
    
    /** Constructor - initialize all values to zero */
    AggregateMetrics()
        : totalDlThroughputMbps(0.0),
          totalUlThroughputMbps(0.0),
          avgDlThroughputMbps(0.0),
          avgUlThroughputMbps(0.0),
          avgSystemDelayMs(0.0),
          totalPacketsSent(0),
          totalPacketsReceived(0),
          totalPacketsLost(0),
          overallPacketLossRate(0.0),
          numUes(0)
    {
    }
};

/**
 * @brief Manager for traffic generation and metrics collection
 * 
 * This class handles:
 * - Traffic application installation (UDP/TCP)
 * - FlowMonitor integration for metrics
 * - Performance metrics collection (throughput, delay, loss)
 * - Per-UE and aggregate statistics
 * 
 * Design: Metrics integrated into TrafficManager to avoid scoping issues
 * and keep traffic generation and measurement together.
 */
class NrTrafficManager : public Object
{
public:
    static TypeId GetTypeId();

    /** Constructor */
    NrTrafficManager();
    
    /** Destructor */
    ~NrTrafficManager() override;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set simulation configuration
     * @param config Pointer to NrSimConfig object
     * 
     * Must be called before InstallTraffic()
     */
    void SetConfig(const Ptr<NrSimConfig>& config);

    /**
     * @brief Set network manager 
     * @param netMgr Pointer to NrNetworkManager
     * 
     * Must be called before InstallTraffic()
     */
    void SetNetworkManager(Ptr<NrNetworkManager> netMgr);

    // ========================================================================
    // TRAFFIC INSTALLATION
    // ========================================================================
    
    /**
     * @brief Install traffic applications on network
     * @param gnbNodes Container of gNB nodes
     * @param ueNodes Container of UE nodes
     * 
     * Creates:
     * - Server applications on remote host
     * - Client applications on UEs
     * - Traffic flows based on configuration
     * - FlowMonitor for metrics collection (automatic)
     */
    void InstallTraffic(const NodeContainer& gnbNodes,
                       const NodeContainer& ueNodes);

    // ========================================================================
    // METRICS COLLECTION
    // ========================================================================
    
    // Enable/disable real-time monitoring
    void EnableRealTimeMonitoring(bool enable, double interval = 1.0);
    
    // Real-time monitoring callback
    void MonitorFlows();
    
    
    // Getters
    std::vector<std::map<uint32_t, PerUeMetrics>> GetTimeSeriesMetrics() const;
    
    /**
     * @brief Collect final metrics after simulation
     * 
     * Call this after Simulator::Run() completes.
     * Processes FlowMonitor stats and computes final averages.
     * 
     * Must be called before GetUeMetrics() or GetAggregateMetrics().
     */
    void CollectMetrics();
    
    /**
     * @brief Get metrics for a specific UE
     * @param ueId UE index (0-based)
     * @return PerUeMetrics structure for the UE
     * 
     * Call after CollectMetrics().
     */
    PerUeMetrics GetUeMetrics(uint32_t ueId) const;
    
    /**
     * @brief Get metrics for all UEs
     * @return Map of UE ID to PerUeMetrics
     * 
     * Call after CollectMetrics().
     */
    std::map<uint32_t, PerUeMetrics> GetAllUeMetrics() const;
    
    /**
     * @brief Get aggregate (system-wide) metrics
     * @return AggregateMetrics structure
     * 
     * Call after CollectMetrics().
     */
    AggregateMetrics GetAggregateMetrics() const;
    
    /**
     * @brief Print metrics summary to console
     * 
     * Convenience method to display results.
     * Call after CollectMetrics().
     */
    void PrintMetricsSummary() const;

    // ========================================================================
    // APPLICATION GETTERS
    // ========================================================================
    
    /**
     * @brief Get downlink client applications
     * @return Container of downlink client applications
     */
    ApplicationContainer GetDlClientApps() const;
    
    /**
     * @brief Get downlink server applications
     * @return Container of downlink server applications
     */                       
    ApplicationContainer GetDlServerApps() const;
    
    /**
     * @brief Get uplink client applications
     * @return Container of uplink client applications
     */                       
    ApplicationContainer GetUlClientApps() const;
    
    /**
     * @brief Get uplink server applications
     * @return Container of uplink server applications
     */                       
    ApplicationContainer GetUlServerApps() const;
      
    // Legacy methods for backward compatibility
    ApplicationContainer GetServerApps() const;
    ApplicationContainer GetClientApps() const;

    // ========================================================================
    // STATE QUERIES
    // ========================================================================
    
    /**
     * @brief Check if traffic has been installed
     * @return True if InstallTraffic() has been called
     */
    bool IsInstalled() const;
    
    /**
     * @brief Check if metrics have been collected
     * @return True if CollectMetrics() has been called
     */
    bool IsCollected() const;

protected:
    void DoDispose() override;

private:
    // ========================================================================
    // TRAFFIC INSTALLATION HELPERS
    // ========================================================================
    
    /**
     * @brief Create remote host and connect to PGW
     * @return Pointer to remote host node
     */
    Ptr<Node> CreateRemoteHost();
    
    /**
     * @brief Install downlink traffic (remote → UEs)
     * @param remoteHost Remote host node
     * @param ueNodes UE nodes
     */
    void InstallDownlinkTraffic(Ptr<Node> remoteHost, const NodeContainer& ueNodes);
    
    /**
     * @brief Install uplink traffic (UEs → remote)
     * @param remoteHost Remote host node
     * @param ueNodes UE nodes
     */
    void InstallUplinkTraffic(Ptr<Node> remoteHost, const NodeContainer& ueNodes);
    
    /**
     * @brief Enable FlowMonitor on all nodes
     */
    void EnableFlowMonitor();

    // ========================================================================
    // METRICS PROCESSING HELPERS
    // ========================================================================
    
    /**
     * @brief Process FlowMonitor statistics
     */
    void ProcessFlowMonitorStats();
    
    /**
     * @brief Determine if flow is downlink or uplink
     * @param flowId Flow ID
     * @return True if downlink, false if uplink
     */
    bool IsDownlinkFlow(FlowId flowId) const;
    
    /**
     * @brief Get UE ID from flow
     * @param flowId Flow ID
     * @return UE ID (0-based), or UINT32_MAX if not found
     */
    uint32_t GetUeIdFromFlow(FlowId flowId) const;
    
    /**
     * @brief Compute aggregate metrics from per-UE data
     */
    void ComputeAggregateMetrics();
    
    /**
     * @brief Initialize per-UE metrics structures
     * @param numUes Number of UEs
     */
    void InitializeUeMetrics(uint32_t numUes);

    // FlowMonitor
    NodeContainer allNodes;
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> m_flowMonitor;
    Ptr<Ipv4FlowClassifier> m_flowClassifier;
    
    // Real-time monitoring
    bool m_enableRealTimeMonitoring;
    double m_monitoringInterval;  // seconds
    bool m_metricsCollected;                    ///< Metrics collected flag
    
    // Metrics storage
    std::map<uint32_t, PerUeMetrics> m_ueMetrics;
    std::map<uint32_t, PerUeMetrics> m_previousMetrics;  // For calculating rates
    AggregateMetrics m_aggregateMetrics;
    
    // Time series data (optional)
    std::vector<std::map<uint32_t, PerUeMetrics>> m_timeSeriesMetrics;

    // ========================================================================
    // MEMBER VARIABLES - Configuration
    // ========================================================================
    
    Ptr<NrSimConfig> m_config;                  ///< Simulation configuration
    Ptr<NrNetworkManager> m_networkManager;     ///< Network manager reference
    
    // ========================================================================
    // MEMBER VARIABLES - Applications
    // ========================================================================
    
    ApplicationContainer m_dlServerApps;        ///< Downlink server apps (remote host)
    ApplicationContainer m_dlClientApps;        ///< Downlink client apps (UEs)
    ApplicationContainer m_ulServerApps;        ///< Uplink server apps (remote host)
    ApplicationContainer m_ulClientApps;        ///< Uplink client apps (UEs)

    // Legacy containers for backward compatibility
    ApplicationContainer m_serverApps;          ///< All server applications
    ApplicationContainer m_clientApps;          ///< All client applications
    
    // ========================================================================
    // MEMBER VARIABLES - State Flags
    // ========================================================================
    
    bool m_installed;                           ///< Traffic installed flag
    
    // ========================================================================
    // MEMBER VARIABLES - Node References
    // ========================================================================
    
    Ptr<Node> m_remoteHost;                     ///< Remote host node
    NodeContainer m_ueNodes;                    ///< UE nodes (stored for metrics)
};

} // namespace ns3

#endif // NR_TRAFFIC_MANAGER_H