/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_TRAFFIC_MANAGER_H
#define NR_TRAFFIC_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "nr-network-manager.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"

namespace ns3
{

class NrSimConfig;

/**
 * @brief Manager for traffic generation and application installation
 * 
 * CURRENT STATUS: Basic stub implementation
 * - Accepts gnbNodes and ueNodes
 * - Does minimal setup
 * 
 * TODO (Future enhancements):
 * - Add remote host parameter
 * - Add IPv4 address handling
 * - Implement UDP/TCP traffic generation
 * - Add per-UE traffic configuration
 * - Support downlink/uplink flows
 */
class NrTrafficManager : public Object
{
  public:
    static TypeId GetTypeId();

    /** Constructor */
    NrTrafficManager();
    
    /** Destructor */
    ~NrTrafficManager() override;

    /**
     * @brief Set simulation configuration
     * @param config Pointer to NrSimConfig object
     * 
     * Must be called before InstallTraffic()
     */
    void SetConfig(const Ptr<NrSimConfig>& config);

    /**
     * @brief Set network manager
     * @param netMgr Pointer to NrNetworkManager object
     */
    void
    SetNetworkManager(Ptr<NrNetworkManager> netMgr);

    /**
     * @brief Install traffic applications on network
     * @param gnbNodes Container of gNB nodes
     * @param ueNodes Container of UE nodes
     * 
     * CURRENT: Stub implementation - does basic setup
     * 
     * FUTURE: Will create:
     * - Server applications on remote host
     * - Client applications on UEs
     * - Traffic flows based on configuration
     */
    void InstallTraffic(const NodeContainer& gnbNodes,
                       const NodeContainer& ueNodes);
    
    /**
     * @brief Report Flow Monitor statistics
     * @param flowMonitor Pointer to FlowMonitor object
     */                       
    void
    ReportFlowMonitorStats(Ptr<FlowMonitor> flowMonitor);                       
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
      
    // legacy methods for backward compatibility
    /**
     * @brief Get server applications
     * @return Container of all server applications
     */
    ApplicationContainer GetServerApps() const;

    /**
     * @brief Get client applications
     * @return Container of all client applications
     */
    ApplicationContainer GetClientApps() const;

    /**
     * @brief Get Flow Monitor
     * @return Pointer to FlowMonitor object
     */
    Ptr<FlowMonitor> GetFlowMonitor() const;

    /**
     * @brief Check if traffic has been installed
     * @return True if InstallTraffic() has been called
     */
    bool IsInstalled() const;

  protected:
    void DoDispose() override;

  private:
    // Member variables
    Ptr<NrSimConfig> m_config;           //!< Simulation configuration
    Ptr<NrNetworkManager> m_networkManager; 
    ApplicationContainer m_dlServerApps;   //!< Downlink server applications container
    ApplicationContainer m_dlClientApps;   //!< Downlink client applications container
    ApplicationContainer m_ulServerApps;   //!< Uplink server applications container
    ApplicationContainer m_ulClientApps;   //!< Uplink client applications container

    // Legacy containers for backward compatibility
    ApplicationContainer m_serverApps;   //!< Server applications container
    ApplicationContainer m_clientApps;   //!< Client applications container

    FlowMonitorHelper m_flowHelper;
    Ptr<FlowMonitor>  m_flowMonitor;

    bool m_installed;                    //!< Installation status flag
};

} // namespace ns3

#endif // NR_TRAFFIC_MANAGER_H