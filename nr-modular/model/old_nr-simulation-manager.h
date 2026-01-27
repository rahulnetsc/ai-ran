/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_SIMULATION_MANAGER_H
#define NR_SIMULATION_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"

#include "utils/nr-sim-config.h"
#include "nr-config-manager.h"
#include "nr-topology-manager.h"
#include "nr-channel-manager.h"
#include "nr-mobility-manager.h"
#include "nr-network-manager.h"
#include "nr-traffic-manager.h"
#include "nr-metrics-manager.h"
#include "nr-output-manager.h"

#include "ns3/nr-helper.h"
#include "ns3/cc-bwp-helper.h"

namespace ns3
{

/**
 * @brief Main orchestrator for NR simulations
 * 
 * Coordinates all sub-managers and provides a clean API for:
 * - Loading/validating configuration
 * - Setting up NR infrastructure
 * - Deploying topology
 * - Configuring channel models
 * - Installing mobility & traffic
 * - Running simulation
 * - Collecting metrics
 */
class NrSimulationManager : public Object
{
  public:
    static TypeId GetTypeId();

    NrSimulationManager();
    ~NrSimulationManager() override;

    /**
     * @brief Set configuration file path
     * @param configPath Path to JSON configuration file
     */
    void SetConfigFile(const std::string& configPath);

    /**
     * @brief Set configuration programmatically
     * @param config Pointer to NrSimConfig object
     */
    void SetConfig(Ptr<NrSimConfig> config);

    /**
     * @brief Initialize simulation (load config, create managers, deploy)
     * 
     * Order of operations:
     * 1. Create all managers
     * 2. Load & validate configuration
     * 3. Setup NR infrastructure (NrHelper, bands)
     * 4. Configure channel models
     * 5. Deploy topology (create nodes, install devices)
     * 6. Install mobility models
     */
    void Initialize();

    /**
     * @brief Run simulation (install traffic, enable metrics, run)
     */
    void Run();

    /**
     * @brief Finalize simulation (collect metrics, write results)
     */
    void Finalize();

    /**
     * @brief Check if simulation has been initialized
     */
    bool IsInitialized() const;

    // Getter methods for sub-managers
    Ptr<NrTopologyManager> GetTopologyManager() const;
    Ptr<NrChannelManager> GetChannelManager() const;
    Ptr<NrMobilityManager> GetMobilityManager() const;
    Ptr<NrTrafficManager> GetTrafficManager() const;
    Ptr<NrMetricsManager> GetMetricsManager() const;
    Ptr<NrOutputManager> GetOutputManager() const;
    Ptr<NrSimConfig> GetConfig() const;
    
    /**
     * @brief Get the NrHelper
     * @return Pointer to NrHelper (null if not initialized)
     */
    Ptr<NrHelper> GetNrHelper() const;
    
    /**
     * @brief Get the operation band
     * @return OperationBandInfo object
     */
    OperationBandInfo GetOperationBand() const;

    /**
     * @brief Create a test remote host node for connectivity tests
     * @return Pointer to created remote host node
     */
    Ptr<Node> CreateTestRemoteHost();
    
  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Create all sub-managers
     */
    void CreateManagers();
    
    /**
     * @brief Setup NR infrastructure (helper, bands)
     */
    void SetupNrInfrastructure();

    // Configuration
    std::string m_configPath;
    Ptr<NrSimConfig> m_config;

    // State flags
    bool m_isInitialized;
    bool m_hasRun;

    // Sub-managers
    Ptr<NrTopologyManager> m_topologyManager;
    Ptr<NrMobilityManager> m_mobilityManager;
    Ptr<NrChannelManager> m_channelManager;
    Ptr<NrNetworkManager> m_networkManager;
    Ptr<NrTrafficManager> m_trafficManager;
    Ptr<NrMetricsManager> m_metricsManager;
    Ptr<NrConfigManager> m_configManager;
    Ptr<NrOutputManager> m_outputManager;
    
    // NR infrastructure
    Ptr<NrHelper> m_nrHelper;
    OperationBandInfo m_operationBand;
};

} // namespace ns3

#endif // NR_SIMULATION_MANAGER_H