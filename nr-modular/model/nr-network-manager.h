/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * NR Network Manager - Header File
 * 
 * Responsibilities:
 * - Setup 5G NR infrastructure (EPC, helpers, channel)
 * - Install NR devices (PHY/MAC/RLC/PDCP stack)
 * - Attach UEs to gNBs (creates GTP tunnels automatically)
 * - Assign IP addresses to UEs
 * - Test network connectivity (ping test)
 * - Test traffic flow (FlowMonitor test)
 */

#ifndef NR_NETWORK_MANAGER_H
#define NR_NETWORK_MANAGER_H

#include "ns3/object.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-address.h"
#include "ns3/cc-bwp-helper.h"

#include <vector>
#include <map>

namespace ns3
{

// Forward declarations
class NrHelper;
class NrPointToPointEpcHelper;
class NrChannelHelper;
class IdealBeamformingHelper;
class NrSimConfig;
class FlowMonitor;

/**
 * \ingroup nr-modular
 * 
 * \brief Manages 5G NR network infrastructure and configuration
 * 
 * This class is responsible for:
 * 1. Creating and configuring the EPC (Evolved Packet Core)
 * 2. Setting up NR helpers and channel models
 * 3. Installing NR devices on gNBs and UEs
 * 4. Attaching UEs to gNBs (automatically creates GTP tunnels)
 * 5. Assigning IP addresses to UEs
 * 6. Testing network connectivity
 * 7. Testing traffic flow capability
 * 
 * Architecture:
 * - EPC creates PGW (Packet Gateway) automatically
 * - GTP tunnels are created during UE attachment
 * - IP addresses are assigned from 7.0.0.0/8 pool
 * - Remote hosts connect to PGW via separate P2P links
 * 
 * Usage:
 * \code
 *   Ptr<NrNetworkManager> netMgr = CreateObject<NrNetworkManager>();
 *   netMgr->SetConfig(config);
 *   
 *   netMgr->SetupNrInfrastructure();
 *   netMgr->InstallNrDevices(gnbs, ues);
 *   netMgr->AttachUesToGnbs();
 *   netMgr->AssignIpAddresses(ues);
 *   
 *   // Optional: Test connectivity before traffic
 *   netMgr->TestConnectivity(remoteHost, ues);
 *   netMgr->TestTrafficFlow(remoteHost, ues, 10.0, 3.0);
 * \endcode
 */
class NrNetworkManager : public Object
{
public:
    /**
     * \brief Get the TypeId for this class
     * \return The TypeId
     */
    static TypeId GetTypeId();
    
    /**
     * \brief Constructor
     */
    NrNetworkManager();
    
    /**
     * \brief Destructor
     */
    virtual ~NrNetworkManager();
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * \brief Set the simulation configuration
     * 
     * Must be called before SetupNrInfrastructure()
     * 
     * \param config Pointer to NrSimConfig object
     */
    void SetConfig(const Ptr<NrSimConfig>& config);
    
    // ========================================================================
    // PHASE 1: INFRASTRUCTURE SETUP
    // ========================================================================
    
    /**
     * \brief Setup the 5G NR infrastructure
     * 
     * This method:
     * 1. Creates NrPointToPointEpcHelper (includes PGW node)
     * 2. Creates NrHelper and links it to EPC
     * 3. Creates and configures channel helper
     * 4. Configures operation band and BWPs
     * 5. Configures beamforming helper
     * 6. Configures antenna models (gNB: 8x8, UE: 2x4)
     * 
     * \param gnbs Container of gNB nodes (must have positions already)
     * \param ues Container of UE nodes (must have positions already)
     * Call this ONCE after setting config, before installing devices
     */
    void SetupNrInfrastructure(const NodeContainer& gnbs, const NodeContainer& ues);
    
    // ========================================================================
    // PHASE 2: DEVICE INSTALLATION
    // ========================================================================
    
    /**
     * \brief Install NR devices on gNBs and UEs
     * 
     * This method:
     * - Installs PHY/MAC/RLC/PDCP stack on gNBs
     * - Installs PHY/MAC/RLC/PDCP stack on UEs
     * - Does NOT assign IP addresses (that comes later)
     * - Does NOT create GTP tunnels (that comes during attachment)
     * 
     * \param gnbs Container of gNB nodes (must have positions already)
     * \param ues Container of UE nodes (must have positions already)
     * 
     * Prerequisites:
     * - SetupNrInfrastructure() must have been called
     * - Nodes must have mobility models installed
     */
    void InstallNrDevices(const NodeContainer& gnbs, const NodeContainer& ues);
    
    // ========================================================================
    // PHASE 3: UE ATTACHMENT
    // ========================================================================
    
    /**
     * \brief Attach UEs to closest gNBs
     * 
     * This method (CRITICAL - lots happens here!):
     * 1. Determines which UE attaches to which gNB (based on distance)
     * 2. Establishes RRC connections
     * 3. Creates GTP tunnels (UE ↔ gNB ↔ PGW) - AUTOMATIC
     * 4. Activates default radio bearers
     * 5. Configures PGW routing tables
     * 
     * Note: All tunnel management is automatic in NS-3!
     * You don't need to manually create GTP tunnels.
     * 
     * Prerequisites:
     * - InstallNrDevices() must have been called
     */
    void AttachUesToGnbs();
    
    // ========================================================================
    // PHASE 4: IP ADDRESS ASSIGNMENT
    // ========================================================================
    
    /**
     * \brief Assign IP addresses to UEs
     * 
     * This method:
     * 1. Installs internet stack on UEs (IPv4 + routing)
     * 2. Assigns IPs from 7.0.0.0/8 pool (automatic via EPC helper)
     *    - UE 0 gets 7.0.0.2
     *    - UE 1 gets 7.0.0.3
     *    - etc.
     * 3. Configures UE routing tables:
     *    - Default route = radio interface (no need to know PGW IP)
     * 4. Updates PGW routing tables:
     *    - 7.0.0.x → GTP tunnel to corresponding gNB
     * 5. Links IP addresses to GTP tunnels created during attachment
     * 
     * \param ues Container of UE nodes
     * 
     * Prerequisites:
     * - AttachUesToGnbs() must have been called
     * 
     * Note: Use AUTOMATIC assignment (via EPC helper) - don't assign manually!
     */
    void AssignIpAddresses(const NodeContainer& ues);

    void AttachUes(Ptr<NrHelper> nrHelper, NetDeviceContainer ueDevices, NetDeviceContainer gnbDevices);
    // ========================================================================
    // HANDOVERs
    // ========================================================================
    /**
     * @brief Enable handover event tracing
     * @param enable True to enable handover traces
     * 
     * Must be called after InstallNrDevices() but before simulation starts
     */
    void EnableHandoverTracing(bool enable = true);

    /**
     * @brief Get total number of handovers in simulation
     * @return Total handover count
     */
    uint32_t GetTotalHandovers() const;
    
    /**
     * @brief Get handovers for specific UE
     * @param ueId UE index (0-based)
     * @return Number of handovers for this UE
     */
    uint32_t GetUeHandoverCount(uint32_t ueId) const;
    
    /**
     * @brief Get current serving gNB for a UE
     * @param ueId UE index
     * @return Cell ID of serving gNB
     */
    uint16_t GetServingGnb(uint32_t ueId) const;
    
    // ========================================================================
    // PHASE 5: CONNECTIVITY TESTING (Optional but Recommended)
    // ========================================================================
    
    /**
     * \brief Test result structure for connectivity test
     */
    struct ConnectivityTestResult
    {
        uint32_t ueIndex;           ///< UE index
        Ipv4Address ueAddress;      ///< UE IP address
        uint32_t packetsSent;       ///< Number of ping packets sent
        uint32_t packetsReceived;   ///< Number of ping responses received
        double avgRttMs;            ///< Average round-trip time (ms)
        double lossPercent;         ///< Packet loss percentage
        bool success;               ///< True if >50% packets received
    };
    
    /**
     * \brief Test network connectivity using ICMP ping
     * 
     * This method:
     * 1. Sends ping packets from remote host to each UE
     * 2. Measures round-trip time (RTT)
     * 3. Calculates packet loss
     * 4. Verifies network layer (Layer 3) connectivity
     * 
     * What this tests:
     * - ✓ IP routing configured correctly
     * - ✓ GTP tunnels functional
     * - ✓ Basic network reachability
     * - ✗ Does NOT test throughput
     * - ✗ Does NOT test radio scheduler
     * 
     * \param remoteHost Node acting as remote host (must be connected to PGW)
     * \param ueNodes Container of UE nodes to test
     * \param testDuration How long to run ping test (seconds, default 1.0)
     * 
     * \return true if all UEs are reachable (>50% ping success), false otherwise
     * 
     * Prerequisites:
     * - AssignIpAddresses() must have been called
     * - Remote host must be connected to PGW (P2P link)
     * - Remote host must have route to 7.0.0.0/8 via PGW
     * 
     * Recommended: Call this before installing traffic to catch config errors early
     */
    bool TestConnectivity(Ptr<Node> remoteHost,
                          const NodeContainer& ueNodes,
                          double testDuration = 1.0);
    
    /**
     * \brief Get detailed results from last connectivity test
     * 
     * \return Vector of test results, one per UE
     */
    std::vector<ConnectivityTestResult> GetConnectivityTestResults() const;
    
    // ========================================================================
    // PHASE 6: TRAFFIC FLOW TESTING (Optional Advanced Test)
    // ========================================================================
    
    /**
     * \brief Test result structure for traffic flow test
     */
    struct FlowTestResult
    {
        uint32_t ueIndex;           ///< UE index
        Ipv4Address ueAddress;      ///< UE IP address
        uint64_t txBytes;           ///< Bytes transmitted
        uint64_t rxBytes;           ///< Bytes received
        uint32_t txPackets;         ///< Packets transmitted
        uint32_t rxPackets;         ///< Packets received
        double throughputMbps;      ///< Achieved throughput (Mbps)
        double avgDelayMs;          ///< Average end-to-end delay (ms)
        double packetLossPercent;   ///< Packet loss percentage
        bool success;               ///< True if achieved >50% of target rate
    };
    
    /**
     * \brief Test actual data flow using FlowMonitor
     * 
     * This method:
     * 1. Installs FlowMonitor on all nodes
     * 2. Creates temporary UDP traffic (downlink)
     * 3. Runs short simulation (default 3 seconds)
     * 4. Measures actual throughput, delay, jitter, loss
     * 5. Verifies application layer (Layer 4+) functionality
     * 
     * What this tests:
     * - ✓ Actual data transfer works
     * - ✓ Throughput achievable
     * - ✓ Radio scheduler functional
     * - ✓ PHY/MAC/RLC layers working
     * - ✓ Detects congestion/interference
     * 
     * \param remoteHost Node acting as remote host (must be connected to PGW)
     * \param ueNodes Container of UE nodes to test
     * \param testRateMbps Target data rate to test (Mbps per UE, default 10.0)
     * \param testDuration How long to run test (seconds, default 3.0)
     * 
     * \return true if all flows achieve >50% of target rate, false otherwise
     * 
     * Prerequisites:
     * - AssignIpAddresses() must have been called
     * - Remote host must be connected to PGW (P2P link)
     * - Remote host must have route to 7.0.0.0/8 via PGW
     * 
     * Note: This is more comprehensive than TestConnectivity() but takes longer
     * Recommended for debugging throughput issues
     */
    bool TestTrafficFlow(Ptr<Node> remoteHost,
                         const NodeContainer& ueNodes,
                         double testRateMbps = 10.0,
                         double testDuration = 3.0);
    
    /**
     * \brief Get detailed results from last traffic flow test
     * 
     * \return Vector of test results, one per flow
     */
    std::vector<FlowTestResult> GetFlowTestResults() const;
    
    // ========================================================================
    // API FOR TRAFFIC MANAGER
    // ========================================================================
    
    /**
     * \brief Get the EPC helper
     * 
     * Traffic manager needs this to:
     * - Get PGW node (for connecting remote hosts)
     * - Access EPC configuration
     * 
     * \return Pointer to EPC helper
     */
    Ptr<NrPointToPointEpcHelper> GetEpcHelper() const;
    
    /**
     * \brief Get UE IP interfaces
     * 
     * Traffic manager needs this to:
     * - Get UE IP addresses for installing traffic applications
     * - Configure traffic destinations
     * 
     * \return Container of UE IPv4 interfaces
     */
    Ipv4InterfaceContainer GetUeIpInterfaces() const;
    
    /**
     * \brief Get gNB devices
     * 
     * \return Container of gNB net devices
     */
    NetDeviceContainer GetGnbDevices() const;
    
    /**
     * \brief Get UE devices
     * 
     * \return Container of UE net devices
     */
    NetDeviceContainer GetUeDevices() const;
    
    /**
     * \brief Get NR helper
     * 
     * Advanced users may need this for:
     * - Custom bearer configuration
     * - PHY/MAC parameter tuning
     * 
     * \return Pointer to NR helper
     */
    Ptr<NrHelper> GetNrHelper() const;
    
    // ========================================================================
    // STATE QUERIES
    // ========================================================================
    
    /**
     * \brief Check if infrastructure has been set up
     * 
     * \return true if SetupNrInfrastructure() has been called
     */
    bool IsSetup() const;
    
    /**
     * \brief Check if devices have been installed
     * 
     * \return true if InstallNrDevices() has been called
     */
    bool IsInstalled() const;
    
    /**
     * \brief Check if UEs have been attached
     * 
     * \return true if AttachUesToGnbs() has been called
     */
    bool IsAttached() const;
    
    /**
     * \brief Check if IP addresses have been assigned
     * 
     * \return true if AssignIpAddresses() has been called
     */
    bool IsAddressAssigned() const;

    void PrintAttachmentStatus() const;

protected:
    /**
     * \brief Destructor implementation
     */
    virtual void DoDispose() override;

private:
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    Ptr<NrSimConfig> m_config;  ///< Simulation configuration
    
    // ========================================================================
    // 5G NR INFRASTRUCTURE
    // ========================================================================
    
    Ptr<NrPointToPointEpcHelper> m_epcHelper;           ///< EPC helper (creates PGW, SGW)
    Ptr<NrHelper> m_nrHelper;                           ///< NR helper (PHY/MAC/RLC/PDCP)
    Ptr<NrChannelHelper> m_channelHelper;               ///< Channel helper (propagation)
    Ptr<IdealBeamformingHelper> m_beamformingHelper;    ///< Beamforming helper
    
    // ========================================================================
    // HANDOVER AND MOBILITY
    // ========================================================================
    std::map<uint32_t, uint16_t> m_ueToGnbMap;  // UE IMSI -> gNB Cell ID
    bool m_enableHandoverTracing;
    uint32_t m_totalHandovers;
    std::map<uint32_t, uint32_t> m_handoverCountPerUe;

    void NotifyConnectionEstablished(std::string context, uint64_t imsi, 
                                    uint16_t cellId, uint16_t rnti);
                                    
    /**
     * @brief Callback for handover start event
     */
    void NotifyHandoverStart(std::string context, uint64_t imsi, 
                        uint16_t sourceCellId, uint16_t rnti, uint16_t targetCellId);
    
    /**
     * @brief Callback for handover end (success)
     */
    void NotifyHandoverEndOk(std::string context, uint64_t imsi, 
                            uint16_t cellId, uint16_t rnti);
    
    /**
     * @brief Callback for handover end (failure)
     */
    void NotifyHandoverEndError(std::string context, uint64_t imsi, 
                               uint16_t cellId, uint16_t rnti);
    
    /**
     * @brief Convert IMSI to UE index
     */
    uint32_t ImsiToUeIndex(uint64_t imsi) const;

    // OPERATION BAND AND BWPs
    // ========================================================================
    
    OperationBandInfo m_operationBand;                  ///< Operation band configuration
    std::vector<OperationBandInfo> m_bands;  // ← Store in vector!          ///< All operation bands
    BandwidthPartInfoPtrVector m_allBwps;               ///< All bandwidth parts
    
    // ========================================================================
    // DEVICES AND INTERFACES
    // ========================================================================
    
    NetDeviceContainer m_gnbDevices;        ///< gNB network devices
    NetDeviceContainer m_ueDevices;         ///< UE network devices
    Ipv4InterfaceContainer m_ueIpInterfaces; ///< UE IP interfaces (7.0.0.x)
    
    // ========================================================================
    // STATE FLAGS
    // ========================================================================
    
    bool m_setup;           ///< True if SetupNrInfrastructure() called
    bool m_installed;       ///< True if InstallNrDevices() called
    bool m_attached;        ///< True if AttachUesToGnbs() called
    bool m_addressAssigned; ///< True if AssignIpAddresses() called
    
    // ========================================================================
    // TEST RESULTS
    // ========================================================================
    
    std::vector<ConnectivityTestResult> m_connectivityResults;  ///< Ping test results
    std::vector<FlowTestResult> m_flowTestResults;              ///< Flow test results
    
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    
    std::map<uint64_t, uint32_t> m_imsiToUeIndexMap;
    
    /**
     * \brief Configure channel model
     * 
     * Called internally by SetupNrInfrastructure()
     */
    void ConfigureChannel();
    
    /**
     * \brief Configure antenna models
     * 
     * Called internally by SetupNrInfrastructure()
     */
    void ConfigureAntennas();
    
    /**
     * \brief Configure beamforming
     * 
     * Called internally by SetupNrInfrastructure()
     */
    void ConfigureBeamforming();
};

} // namespace ns3

#endif /* NR_NETWORK_MANAGER_H */