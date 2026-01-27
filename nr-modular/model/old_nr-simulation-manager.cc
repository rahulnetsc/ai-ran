/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

// ns-3 includes
#include "nr-simulation-manager.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/point-to-point-helper.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/simulator.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("NrSimulationManager");
NS_OBJECT_ENSURE_REGISTERED (NrSimulationManager);  

TypeId
NrSimulationManager::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::NrSimulationManager")
        .SetParent<Object> ()
        .SetGroupName("NrModular")
        .AddConstructor<NrSimulationManager> ();
    return tid;     
}

NrSimulationManager::NrSimulationManager()
    : m_configPath(""),
      m_config(nullptr),
      m_isInitialized(false),
      m_hasRun(false),
      m_topologyManager(nullptr),
      m_mobilityManager(nullptr),
      m_channelManager(nullptr),
      m_trafficManager(nullptr),
      m_metricsManager(nullptr),
      m_configManager(nullptr),
      m_outputManager(nullptr)
{
    NS_LOG_FUNCTION(this);
}

NrSimulationManager::~NrSimulationManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrSimulationManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_topologyManager = nullptr;
    m_mobilityManager = nullptr;
    m_channelManager = nullptr;
    m_trafficManager = nullptr; 
    m_metricsManager = nullptr;
    m_configManager = nullptr;
    m_outputManager = nullptr;
    m_config = nullptr;
    Object::DoDispose();
}

void NrSimulationManager::SetConfigFile(const std::string& configPath)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(m_isInitialized, 
        "Cannot set config file after initialization!");
    m_configPath = configPath;
    NS_LOG_INFO("Configuration file set to: " << m_configPath);
}

void 
NrSimulationManager::SetConfig(Ptr<NrSimConfig> config)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(m_isInitialized, 
        "Cannot set config after initialization!");
    m_config = config;
    NS_LOG_INFO("Configuration set programmatically.");
}


void NrSimulationManager::CreateManagers()
{
    m_configManager = CreateObject<NrConfigManager>();
    m_topologyManager = CreateObject<NrTopologyManager>();
    m_networkManager = CreateObject<NrNetworkManager>();    // NEW!
    m_channelManager = CreateObject<NrChannelManager>();
    m_mobilityManager = CreateObject<NrMobilityManager>();
    m_trafficManager = CreateObject<NrTrafficManager>();
    m_metricsManager = CreateObject<NrMetricsManager>();
    m_outputManager = CreateObject<NrOutputManager>();
}

Ptr<Node>
NrSimulationManager::CreateTestRemoteHost()
{
    NS_LOG_FUNCTION(this);
    
    // Create remote host
    Ptr<Node> remoteHost = CreateObject<Node>();
    
    // Install internet stack
    InternetStackHelper internet;
    internet.Install(remoteHost);
    
    // Get PGW
    Ptr<Node> pgw = m_networkManager->GetEpcHelper()->GetPgwNode();
    
    // Create P2P link to PGW
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    
    NodeContainer internetNodes(pgw, remoteHost);
    NetDeviceContainer internetDevices = p2p.Install(internetNodes);
    
    // Assign IPs (1.0.0.0/8)
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIps = ipv4h.Assign(internetDevices);
    
    // Configure routing (7.0.0.0/8 via PGW)
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> remoteRouting = 
        routingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    
    remoteRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"),
        Ipv4Mask("255.0.0.0"),
        internetIps.GetAddress(0),  // PGW
        1
    );
    
    return remoteHost;
}

void
NrSimulationManager::Initialize()
{
    NS_LOG_FUNCTION(this);
    
    NS_ABORT_MSG_IF(m_isInitialized, "Initialize() called twice!");
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Initializing NR Simulation");
    NS_LOG_INFO("========================================");
    
    // =================================================================
    // STEP 1: Create all managers
    // =================================================================
    NS_LOG_INFO("Step 1/10: Creating managers...");
    CreateManagers();
    
    // =================================================================
    // STEP 2: Load configuration
    // =================================================================
    NS_LOG_INFO("Step 2/10: Loading configuration...");
    
    if (m_config == nullptr)
    {
        // Config not set directly, must load from file
        NS_ABORT_MSG_IF(m_configPath.empty(),
                        "No config file set! Call SetConfigFile() or SetConfig()");
        
        m_config = m_configManager->LoadFromFile(m_configPath);
    }
    
    // Print configuration
    NS_LOG_INFO("Configuration loaded:");
    m_config->Print(std::cout);
    
    // =================================================================
    // STEP 3: Validate configuration
    // =================================================================
    NS_LOG_INFO("Step 3/10: Validating configuration...");
    m_configManager->ValidateOrAbort(m_config);
    
    // =================================================================
    // STEP 4: Set config on all managers
    // =================================================================
    NS_LOG_INFO("Step 4/10: Distributing config to managers...");
    
    m_topologyManager->SetConfig(m_config);
    m_channelManager->SetConfig(m_config);
    m_mobilityManager->SetConfig(m_config);
    m_trafficManager->SetConfig(m_config);
    m_metricsManager->SetConfig(m_config);
    m_outputManager->SetConfig(m_config);
    
    std::cout << "Step 5/10: Setting up NR infrastructure..." << std::endl;
    m_networkManager->SetConfig(m_config);
    m_networkManager->SetupNrInfrastructure();
    
    // STEP 6: Deploy Topology
    std::cout << "Step 6/10: Deploying topology..." << std::endl;
    m_topologyManager->DeployTopology();
    
    NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
    NodeContainer ueNodes = m_topologyManager->GetUeNodes();
    
    // STEP 7: Install NR Devices
    std::cout << "Step 7/10: Installing NR devices..." << std::endl;
    m_networkManager->InstallNrDevices(gnbNodes, ueNodes);
    
    // STEP 8: Attach UEs
    std::cout << "Step 8/10: Attaching UEs to gNBs..." << std::endl;
    m_networkManager->AttachUesToGnbs();
    
    // STEP 9: Assign IP Addresses
    std::cout << "Step 9/10: Assigning IP addresses..." << std::endl;
    m_networkManager->AssignIpAddresses(ueNodes);
    
    // STEP 10: Test Network Connectivity (OPTIONAL)
    if (m_config->enableConnectivityTest)
    {
        std::cout << "\nStep 10/10: Testing network connectivity..." << std::endl;
        
        Ptr<Node> testRemoteHost = CreateTestRemoteHost();
        
        bool connectivityOk = m_networkManager->TestConnectivity(
            testRemoteHost, ueNodes, 1.0);
        
        if (!connectivityOk)
        {
            NS_LOG_WARN("⚠ Connectivity test failed!");
            // Optionally abort:
            // NS_ABORT_MSG("Network connectivity test failed!");
        }
        else
        {
            std::cout << "✓ Network connectivity verified!" << std::endl;
        }
    }
    else
    {
        std::cout << "Step 10/10: Skipping connectivity test (disabled)" << std::endl;
    }


    // Install Mobility (AFTER network devices)
    m_mobilityManager->SetConfig(m_config);
    m_mobilityManager->InstallUeMobility(ueNodes);
    // =================================================================
    // DONE
    // =================================================================
    
    m_isInitialized = true;
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Initialization Complete!");
    NS_LOG_INFO("========================================");
}

void
NrSimulationManager::Run()
{
    NS_LOG_FUNCTION(this);
    
    NS_ABORT_MSG_IF(!m_isInitialized, 
                    "Must call Initialize() before Run()!");
    NS_ABORT_MSG_IF(m_hasRun, 
                    "Run() called twice!");
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Running Simulation");
    NS_LOG_INFO("========================================");
    
    NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
    NodeContainer ueNodes = m_topologyManager->GetUeNodes();

    m_trafficManager->SetNetworkManager(m_networkManager);
    
    // =================================================================
    // Install traffic applications
    // =================================================================
    NS_LOG_INFO("Installing traffic...");
    m_trafficManager->InstallTraffic(gnbNodes, ueNodes);
    
    // =================================================================
    // Enable metrics collection
    // =================================================================
    NS_LOG_INFO("Enabling metrics...");
    m_metricsManager->EnableMetrics(gnbNodes, ueNodes);
    
    // =================================================================
    // Run the simulation
    // =================================================================
    double simDuration = m_config->simDuration;
    
    NS_LOG_INFO("Starting simulation for " << simDuration << " seconds...");
    
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    
    NS_LOG_INFO("Simulation complete!");
    
    m_hasRun = true;
    
    NS_LOG_INFO("========================================");
}

void
NrSimulationManager::Finalize()
{
    NS_LOG_FUNCTION(this);
    
    NS_ABORT_MSG_IF(!m_hasRun, 
                    "Must call Run() before Finalize()!");
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Finalizing Simulation");
    NS_LOG_INFO("========================================");
    
    // =================================================================
    // Collect final metrics
    // =================================================================
    NS_LOG_INFO("Collecting metrics...");
    m_metricsManager->CollectFinalMetrics();
    
    // =================================================================
    // Write results to file
    // =================================================================
    NS_LOG_INFO("Writing results...");
    m_outputManager->WriteResults();
    
    // =================================================================
    // Cleanup simulator
    // =================================================================
    NS_LOG_INFO("Destroying simulator...");
    Simulator::Destroy();
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Simulation Finalized");
    NS_LOG_INFO("========================================");
}

bool
NrSimulationManager::IsInitialized() const
{
    return m_isInitialized;
}

Ptr<NrTopologyManager>
NrSimulationManager::GetTopologyManager() const
{
    return m_topologyManager;
}

Ptr<NrChannelManager>
NrSimulationManager::GetChannelManager() const
{
    return m_channelManager;
}

Ptr<NrMobilityManager>
NrSimulationManager::GetMobilityManager() const
{
    return m_mobilityManager;
}

Ptr<NrTrafficManager>
NrSimulationManager::GetTrafficManager() const
{
    return m_trafficManager;
}

Ptr<NrMetricsManager>
NrSimulationManager::GetMetricsManager() const
{
    return m_metricsManager;
}

Ptr<NrOutputManager>
NrSimulationManager::GetOutputManager() const
{
    return m_outputManager;
}

Ptr<NrSimConfig>
NrSimulationManager::GetConfig() const
{
    return m_config;
}

} // namespace ns3