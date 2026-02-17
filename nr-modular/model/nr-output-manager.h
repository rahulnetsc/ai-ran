/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_OUTPUT_MANAGER_H
#define NR_OUTPUT_MANAGER_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/vector.h"
#include "ns3/ipv4-address.h"

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <fstream>
#include <chrono>

namespace ns3 {

// Forward declarations - Core ns-3
class Node;
class NodeContainer;
class MobilityModel;

// Forward declarations - Our managers
class NrSimConfig;
class NrTopologyManager;
class NrNetworkManager;
class NrTrafficManager;
class NrMetricsManager;
class NrChannelManager;
class NrMobilityManager;
// class NrBwpManager;

/**
 * \ingroup nr-modular
 * \brief Manages all simulation outputs including file-based results and real-time telemetry
 *
 * This class handles three types of outputs:
 * 1. Traditional file-based results (CSV, TXT)
 * 2. Real-time state telemetry (JSON over UDP/TCP/File)
 * 3. Simulation logs and event traces
 *
 * Features:
 * - Periodic state publishing for external controllers
 * - Event-triggered updates (handovers, attachments, etc.)
 * - Configurable metrics inclusion
 * - Multiple output formats and destinations
 * - Historical state buffering
 * - Simulation replay support (future)
 */
class NrOutputManager : public Object
{
public:
    /**
     * \brief Get the TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * \brief Constructor
     */
    NrOutputManager();

    /**
     * \brief Destructor
     */
    virtual ~NrOutputManager();

    // ================================================================
    // CONFIGURATION
    // ================================================================

    /**
     * \brief Set the simulation configuration
     * \param config Pointer to the simulation configuration
     */
    void SetConfig(Ptr<NrSimConfig> config);

    /**
     * \brief Set reference to other managers for state collection
     * \param topology TopologyManager instance
     * \param network NetworkManager instance
     * \param traffic TrafficManager instance
     * \param metrics MetricsManager instance
     * \param channel ChannelManager instance (optional)
     * \param mobility MobilityManager instance (optional)
     */
    void SetManagers(Ptr<NrTopologyManager> topology,
                    Ptr<NrNetworkManager> network,
                    Ptr<NrTrafficManager> traffic,
                    Ptr<NrMetricsManager> metrics,
                    Ptr<NrChannelManager> channel = nullptr,
                    Ptr<NrMobilityManager> mobility = nullptr
                    // Ptr<NrBwpManager> bwp = nullptr
                    );
        
    // ================================================================
    // TRADITIONAL FILE OUTPUT (PHASE 1 - CURRENT)
    // ================================================================

    /**
     * \brief Write final simulation results to file
     * 
     * Writes traditional end-of-simulation results in configured format
     */
    void WriteResults();

    /**
     * \brief Write results to specific file path
     * \param filepath Path to output file
     */
    void WriteResultsToFile(const std::string& filepath);

    // ================================================================
    // REAL-TIME TELEMETRY (PHASE 2 - NEW)
    // ================================================================

    /**
     * \brief Initialize real-time telemetry system
     * 
     * Sets up periodic state publishing and event handlers
     */
    void InitializeTelemetry();

    /**
     * \brief Start real-time state publishing
     * \param interval Update interval in seconds
     */
    void StartTelemetry(double interval = 0.1);

    /**
     * \brief Stop real-time state publishing
     */
    void StopTelemetry();

    /**
     * \brief Manually trigger immediate state publication
     * \param eventType Type of event triggering this update
     */
    void PublishStateNow(const std::string& eventType = "manual");

    // ================================================================
    // EVENT HANDLERS (For event-triggered updates)
    // ================================================================

    /**
     * \brief Called when a UE attaches to a cell
     * \param ueId UE index
     * \param cellId Cell ID
     */
    void OnUeAttachment(uint32_t ueId, uint16_t cellId);

    /**
     * \brief Called when a handover completes
     * \param ueId UE index
     * \param sourceCellId Source cell ID
     * \param targetCellId Target cell ID
     * \param success Whether handover succeeded
     */
    void OnHandover(uint32_t ueId, uint16_t sourceCellId, uint16_t targetCellId, bool success);

    /**
     * \brief Called when traffic metrics update
     * \param ueId UE index
     */
    void OnTrafficUpdate(uint32_t ueId);

    // ================================================================
    // STATE COLLECTION
    // ================================================================

    /**
     * \brief Structure representing complete simulation state
     */
    struct SimulationState
    {
        // Timestamp information
        double simulationTime;      ///< Simulation time in seconds
        std::string wallClockTime;  ///< Real-world timestamp
        uint64_t wallClockSeconds;  ///< Seconds since simulation start
        
        // Simulation status
        std::string status;         ///< "initializing", "running", "finalizing"
        double progressPercent;     ///< Progress percentage (0-100)
        double totalDuration;       ///< Total simulation duration
        
        // Network topology
        uint32_t gnbCount;
        uint32_t ueCount;
        
        // Per-UE state
        struct UeState
        {
            uint32_t ueId;
            uint64_t imsi;
            Vector position;
            Vector velocity;
            double speed;
            std::string mobilityModel;
            uint32_t currentWaypoint;
            uint32_t totalWaypoints;
            
            // Network attachment
            uint16_t cellId;
            uint16_t gnbId;
            double distanceToGnb;
            
            // Radio measurements (if available)
            bool hasRadioMetrics;
            double rsrpDbm;
            double sinrDb;
            uint8_t cqi;
            uint8_t mcs;
            
            // Traffic stats
            double dlThroughputMbps;
            double ulThroughputMbps;
            uint64_t dlPacketsTx;
            uint64_t dlPacketsRx;
            uint64_t ulPacketsTx;
            uint64_t ulPacketsRx;
            double dlLossPct;
            double ulLossPct;
            double avgDelayMs;

            // BWP assignment (bandwidth part)
            uint32_t currentBwpId;
            double bwpCenterFrequencyHz;
            double bwpBandwidthHz;
            uint32_t bwpNumerology;
            
            // Buffer status (if available)
            bool hasBufferMetrics;
            uint64_t ulBufferBytes;
            uint64_t dlBufferBytes;
        };
        std::vector<UeState> ues;
        
        // Per-gNB state
        struct GnbState
        {
            uint32_t gnbId;
            Vector position;
            uint16_t cellId;
            uint32_t attachedUeCount;
            std::vector<uint32_t> attachedUeIds;
            
            // Scheduler information
            std::string scheduler_type;  // e.g. "ns3::NrMacSchedulerTdmaRR"
            
            // Resource utilization (if available)
            bool hasSchedulerMetrics;
            double resourceUtilizationPct;
            uint32_t allocatedRbs;
            uint32_t totalRbs;
            
            // Buffer status (if available)
            bool hasBufferMetrics;
            uint64_t dlQueueBytes;
            uint64_t dlQueuePackets;
        };
        std::vector<GnbState> gnbs;
        
        // Aggregate traffic stats
        double totalDlThroughputMbps;
        double totalUlThroughputMbps;
        double avgPacketLossPct;
        
        // Handover summary
        uint32_t totalHandovers;
        
        // Recent handover events (circular buffer)
        struct HandoverEvent
        {
            double timestamp;
            uint32_t ueId;
            uint16_t sourceCellId;
            uint16_t targetCellId;
            bool success;
            std::string reason;
        };
        std::deque<HandoverEvent> recentHandovers;

        // BWP Configuration (static - sent once or with every state)
        struct BwpConfigInfo
        {
            uint32_t bwpId;
            double centerFrequencyHz;
            double bandwidthHz;
            double frequencyStartHz;
            double frequencyEndHz;
            uint32_t numerology;
            double subcarrierSpacingKhz;
            uint32_t numResourceBlocks;
            std::string description;
            std::string colorHex;
        };

        struct BwpConfiguration
        {
            uint32_t numBwps;
            std::vector<BwpConfigInfo> bwps;
        };

        struct BwpStats
        {
            std::vector<uint32_t> ueCountPerBwp;
            std::map<uint32_t, uint32_t> assignments;  // ueId → bwpId
        };

        BwpConfiguration bwpConfiguration;
        BwpStats bwpStats;
        
        // Event log (circular buffer)
        struct SimulationEvent
        {
            double timestamp;
            std::string type;       // "attachment", "handover", "traffic_alert", etc.
            std::string description;
            std::map<std::string, std::string> details;
        };
        std::deque<SimulationEvent> recentEvents;
    };


    /**
     * \brief Collect current simulation state
     * \return Complete simulation state snapshot
     */
    SimulationState CollectCurrentState();

    /**
     * \brief Get historical states (if buffering enabled)
     * \param count Number of historical states to return (0 = all)
     * \return Vector of historical states
     */
    std::vector<SimulationState> GetStateHistory(uint32_t count = 0);

    /**
     * \brief Collect BWP configuration (static info)
     */
    void CollectBwpConfiguration(SimulationState& state);

    /**
     * \brief Collect BWP statistics (dynamic assignments)
     */
    void CollectBwpStats(SimulationState& state);

    // ================================================================
    // OUTPUT FORMATTING
    // ================================================================

    /**
     * \brief Convert state to JSON string
     * \param state Simulation state to convert
     * \param prettyPrint Whether to format JSON with indentation
     * \return JSON string
     */
    std::string StateToJson(const SimulationState& state, bool prettyPrint = false);

    /**
     * \brief Convert state to CSV rows
     * \param state Simulation state to convert
     * \return Vector of CSV rows
     */
    std::vector<std::string> StateToCsv(const SimulationState& state);

    /**
     * \brief Generate summary report
     * \param state Simulation state
     * \return Human-readable summary text
     */
    std::string GenerateSummaryReport(const SimulationState& state);

    // ================================================================
    // PUBLISHING MECHANISMS
    // ================================================================

    /**
     * \brief Publishing method enumeration
     */
    enum PublishMethod
    {
        PUBLISH_FILE,       ///< Write to file
        PUBLISH_UDP,        ///< Send via UDP socket
        PUBLISH_TCP,        ///< Send via TCP socket
        PUBLISH_PIPE,       ///< Write to named pipe (UNIX)
        PUBLISH_DISABLED    ///< Telemetry disabled
    };

    /**
     * \brief Configure publishing method
     * \param method Publishing method to use
     * \param host Hostname/IP (for UDP/TCP)
     * \param port Port number (for UDP/TCP)
     * \param filepath File path (for FILE/PIPE)
     */
    void ConfigurePublishing(PublishMethod method,
                           const std::string& host = "localhost",
                           uint16_t port = 5555,
                           const std::string& filepath = "/tmp/nr_sim_state.json");

    // ================================================================
    // TELEMETRY CONFIGURATION
    // ================================================================

    /**
     * \brief Enable/disable specific metric groups
     */
    struct TelemetryConfig
    {
        bool includePositions;      ///< Include UE/gNB positions
        bool includeVelocities;     ///< Include UE velocities
        bool includeAttachments;    ///< Include attachment info
        bool includeTrafficStats;   ///< Include throughput/loss
        bool includeHandovers;      ///< Include handover events
        bool includeRadioMetrics;   ///< Include RSRP/SINR (if available)
        bool includeBufferMetrics;  ///< Include queue states (if available)
        bool includeSchedulerMetrics; ///< Include scheduler info (if available)
        bool includeEventLog;       ///< Include event history
        
        uint32_t maxHistorySize;    ///< Max historical states to keep
        uint32_t maxHandoverHistory; ///< Max handover events to keep
        uint32_t maxEventHistory;   ///< Max general events to keep
        
        bool eventTriggeredUpdates; ///< Publish on events (in addition to periodic)
        
        TelemetryConfig()
            : includePositions(true),
              includeVelocities(true),
              includeAttachments(true),
              includeTrafficStats(true),
              includeHandovers(true),
              includeRadioMetrics(false),
              includeBufferMetrics(false),
              includeSchedulerMetrics(false),
              includeEventLog(true),
              maxHistorySize(100),
              maxHandoverHistory(50),
              maxEventHistory(100),
              eventTriggeredUpdates(true)
        {}
    };

    /**
     * \brief Set telemetry configuration
     * \param config Telemetry configuration
     */
    void SetTelemetryConfig(const TelemetryConfig& config);

    /**
     * \brief Get current telemetry configuration
     * \return Telemetry configuration
     */
    TelemetryConfig GetTelemetryConfig() const;

    // ================================================================
    // STATISTICS & DIAGNOSTICS
    // ================================================================

    /**
     * \brief Get number of states published
     * \return Count of published states
     */
    uint64_t GetPublishedStateCount() const;

    /**
     * \brief Get number of failed publications
     * \return Count of failed publications
     */
    uint64_t GetFailedPublishCount() const;

    /**
     * \brief Get average state generation time
     * \return Average time in milliseconds
     */
    double GetAvgStateGenerationTimeMs() const;

    /**
     * \brief Get average JSON size
     * \return Average size in bytes
     */
    uint64_t GetAvgJsonSizeBytes() const;

    /**
     * \brief Print telemetry statistics
     */
    void PrintTelemetryStats() const;

protected:
    virtual void DoDispose() override;

private:
    // ================================================================
    // INTERNAL STATE COLLECTION METHODS
    // ================================================================

    /**
     * \brief Collect UE position and mobility state
     */
    SimulationState::UeState CollectUeState(uint32_t ueId);

    /**
     * \brief Collect gNB state
     */
    SimulationState::GnbState CollectGnbState(uint32_t gnbId);

    /**
     * \brief Collect traffic statistics for a UE
     */
    void CollectUeTrafficStats(SimulationState::UeState& ueState);

    /**
     * \brief Collect radio measurements for a UE (if available)
     */
    void CollectUeRadioMetrics(SimulationState::UeState& ueState);

    /**
     * \brief Collect buffer status for a UE (if available)
     */
    void CollectUeBufferMetrics(SimulationState::UeState& ueState);

    /**
     * \brief Collect aggregate traffic statistics
     */
    void CollectAggregateStats(SimulationState& state);

    /**
     * \brief Collect handover history
     */
    void CollectHandoverHistory(SimulationState& state);

    // ================================================================
    // PUBLISHING METHODS
    // ================================================================

    /**
     * \brief Periodic state publishing callback
     */
    void PeriodicPublish();

    /**
     * \brief Publish state via configured method
     */
    void PublishState(const SimulationState& state, const std::string& trigger);

    /**
     * \brief Publish to file
     */
    bool PublishToFile(const std::string& json, const std::string& filepath);

    /**
     * \brief Publish via UDP socket
     */
    bool PublishViaUdp(const std::string& json);

    /**
     * \brief Publish via TCP socket
     */
    bool PublishViaTcp(const std::string& json);

    /**
     * \brief Publish to named pipe
     */
    bool PublishToPipe(const std::string& json);

    // ================================================================
    // UTILITY METHODS
    // ================================================================

    /**
     * \brief Calculate distance between two points
     */
    double CalculateDistance(const Vector& pos1, const Vector& pos2);

    /**
     * \brief Get current wall clock time as ISO8601 string
     */
    std::string GetCurrentTimeIso8601();

    /**
     * \brief Add event to event log
     */
    void LogEvent(const std::string& type, const std::string& description);

    /**
     * \brief Schedule next periodic update
     */
    void ScheduleNextUpdate();

    // ================================================================
    // MEMBER VARIABLES
    // ================================================================

    // Configuration
    Ptr<NrSimConfig> m_config;              ///< Simulation configuration
    TelemetryConfig m_telemetryConfig;      ///< Telemetry-specific config

    // Manager references
    Ptr<NrTopologyManager> m_topologyManager;
    Ptr<NrNetworkManager> m_networkManager;
    Ptr<NrTrafficManager> m_trafficManager;
    Ptr<NrMetricsManager> m_metricsManager;
    Ptr<NrChannelManager> m_channelManager;
    Ptr<NrMobilityManager> m_mobilityManager;
    // Ptr<NrBwpManager> m_bwpManager;

    // Telemetry state
    bool m_telemetryEnabled;                ///< Is telemetry active
    bool m_telemetryInitialized;            ///< Has telemetry been initialized
    EventId m_publishEvent;                 ///< Scheduled publish event
    Time m_publishInterval;                 ///< Time between publishes
    Time m_lastPublishTime;                 ///< Last publish timestamp

    // Publishing configuration
    PublishMethod m_publishMethod;          ///< Current publishing method
    std::string m_publishHost;              ///< Target host for UDP/TCP
    uint16_t m_publishPort;                 ///< Target port for UDP/TCP
    std::string m_publishFilepath;          ///< File path for FILE/PIPE

    // Socket handles (for UDP/TCP)
    int m_udpSocket;                        ///< UDP socket descriptor
    int m_tcpSocket;                        ///< TCP socket descriptor
    bool m_tcpConnected;                    ///< TCP connection status

    // State history
    std::deque<SimulationState> m_stateHistory;  ///< Historical states

    // Event tracking
    std::deque<SimulationState::HandoverEvent> m_handoverEvents;
    std::deque<SimulationState::SimulationEvent> m_eventLog;

    // Statistics
    uint64_t m_publishedStateCount;         ///< Count of published states
    uint64_t m_failedPublishCount;          ///< Count of failed publishes
    std::vector<double> m_stateGenTimes;    ///< State generation times
    std::vector<uint64_t> m_jsonSizes;      ///< JSON sizes

    // BWP tracking
    bool m_bwpConfigurationSent;  ///< True if static BWP config already sent

    // Timing
    Time m_simulationStartTime;             ///< Wall clock time at sim start
    std::chrono::steady_clock::time_point m_wallClockStart;  ///< Real-world start time

    // File handle (for file output)
    std::ofstream m_outputFile;             ///< Output file stream
};

} // namespace ns3

#endif /* NR_OUTPUT_MANAGER_H */