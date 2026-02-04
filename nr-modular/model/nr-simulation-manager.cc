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
    std::cout << "Configuration file set to: " << m_configPath << std::endl;
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

    auto start = std::chrono::high_resolution_clock::now();
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
    std::cout << "m_config->topology.useFilePositions = " << (m_config->topology.useFilePositions ? "true" : "false") << std::endl;
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
    
    // STEP 5: Deploy Topology
    std::cout << "Step 5/10: Deploying topology..." << std::endl;
    m_topologyManager->DeployTopology();
    
    NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
    NodeContainer ueNodes = m_topologyManager->GetUeNodes();
    
    // STEP 6: Install mobility on UEs, gNBs have static mobility
    // std::cout << "Step 6/10: Installing gNB mobility..." << std::endl;
    // m_mobilityManager->InstallGnbMobility(gnbNodes);
    std::cout << "Step 6/10: Installing UE mobility..." << std::endl;
    m_mobilityManager->InstallUeMobility(ueNodes);

    // STEP 7: Setup NR Infrastructure, install devices, attach UEs, assign IPs
    std::cout << "Step 7/10: Setting up NR infrastructure..." << std::endl;
    m_networkManager->SetConfig(m_config);
    m_networkManager->SetupNrInfrastructure(gnbNodes, ueNodes);
    
    // STEP 8: Assign IP Addresses
    std::cout << "Step 8/10: Assigning IP addresses and attaching UEs..." << std::endl;
    m_networkManager->AssignIpAddresses(ueNodes);

    // ⭐ NEW: Enable handover tracking (if you have multiple gNBs)
    if (m_config->topology.gnbCount > 1)
    {
        std::cout << "\nEnabling handover tracking..." << std::endl;
        m_networkManager->EnableHandoverTracing(true);
    }
    
    // NEW STEP: Now attach after positions are set
    std::cout << "Step 8b/10: Attaching UEs to closest gNBs..." << std::endl;
    m_networkManager->AttachUes(
        m_networkManager->GetNrHelper(),
        m_networkManager->GetUeDevices(),
        m_networkManager->GetGnbDevices()
    );

    // =================================================================
    // DONE
    // =================================================================
    
    m_isInitialized = true;
    
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Initialization Complete!");
    NS_LOG_INFO("========================================");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    NS_LOG_INFO("Initialization Time: " << duration << " seconds");
    std::cout << "Initialization Time: " << duration << " seconds" << std::endl;
}

void
NrSimulationManager::Run()
{
    NS_LOG_FUNCTION(this);
    
    NS_ABORT_MSG_IF(!m_isInitialized, 
                    "Must call Initialize() before Run()!");
    NS_ABORT_MSG_IF(m_hasRun, 
                    "Run() called twice!");
    auto start = std::chrono::high_resolution_clock::now();
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Running Simulation");
    NS_LOG_INFO("========================================");
    
    NodeContainer gnbNodes = m_topologyManager->GetGnbNodes();
    NodeContainer ueNodes = m_topologyManager->GetUeNodes();

    m_trafficManager->SetNetworkManager(m_networkManager);
    
    // =================================================================
    // Install traffic applications
    // =================================================================
    NS_LOG_INFO("STEP 9/10: Installing traffic...");
    m_trafficManager->InstallTraffic(gnbNodes, ueNodes);

    NS_LOG_INFO("STEP 9b/10: Enabling real-time traffic monitoring...");
    std::cout << "Enabling real-time traffic monitoring..." << std::endl;
    m_trafficManager->EnableRealTimeMonitoring(m_config->monitoring.monitorInterval);
    
    // =================================================================
    // Setup Output Manager
    // =================================================================
    NS_LOG_INFO("Step 10/10: Setting up Output Manager...");
    std::cout << "Step 10/10: Setting up Output Manager..." << std::endl;

    m_outputManager->SetManagers(
        m_topologyManager,
        m_networkManager,
        m_trafficManager,
        m_metricsManager
    );

    m_outputManager->InitializeTelemetry();

    // Configure for UDP publishing
    m_outputManager->ConfigurePublishing(
        NrOutputManager::PUBLISH_UDP,
        "127.0.0.1",
        5555
    );

    // Start with 100ms updates
    m_outputManager->StartTelemetry(0.1);
    // =================================================================
    // Run the simulation
    // =================================================================
    double simDuration = m_config->simDuration;
    
    NS_LOG_INFO("Starting simulation for " << simDuration << " seconds...");
    std::cout << "Starting simulation for " << simDuration << " seconds..." << std::endl;
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    
    NS_LOG_INFO("Simulation complete!");
    std::cout << "Simulation complete!" << std::endl;
    m_hasRun = true;
    
    NS_LOG_INFO("========================================");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    NS_LOG_INFO("Simulation Run Time: " << duration << " seconds");
    std::cout << "Simulation Run Time: " << duration << " seconds" << std::endl;
}

void
NrSimulationManager::Finalize()
{
    NS_LOG_FUNCTION(this);
    
    NS_ABORT_MSG_IF(!m_hasRun, 
                    "Must call Run() before Finalize()!");
    auto start = std::chrono::high_resolution_clock::now();
    NS_LOG_INFO("========================================");
    NS_LOG_INFO("Finalizing Simulation");
    NS_LOG_INFO("========================================");
    
    // =================================================================
    // Print Handover summary
    // =================================================================
    if ((m_config->topology.gnbCount > 1) && (m_config->debug.enableVerboseHandoverLogs))
    {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Handover Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // ✅ NEW: Print real-time attachment status first
        m_networkManager->PrintAttachmentStatus();
        
        uint32_t totalHandovers = m_networkManager->GetTotalHandovers();
        std::cout << "Total handovers: " << totalHandovers << std::endl;
        
        std::cout << "\nPer-UE handovers:" << std::endl;
        NodeContainer ueNodes = m_topologyManager->GetUeNodes();
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            uint32_t count = m_networkManager->GetUeHandoverCount(i);
            uint16_t servingGnb = m_networkManager->GetServingGnb(i);
            std::cout << "  UE " << i << ": " << count << " handovers "
                      << "(final gNB: " << servingGnb << ")" << std::endl;
        }
        std::cout << "========================================\n" << std::endl;
    }

    // =================================================================
    // Collect final metrics
    // =================================================================
    NS_LOG_INFO("Collecting metrics...");
    m_trafficManager->CollectMetrics();
    m_trafficManager->PrintMetricsSummary();  // ← ADD THIS!

    

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

    


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    NS_LOG_INFO("Finalization Time: " << duration << " seconds");
    std::cout << "Finalization Time: " << duration << " seconds" << std::endl;
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

Ptr<NrNetworkManager>
NrSimulationManager::GetNetworkManager() const
{
    return m_networkManager;
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