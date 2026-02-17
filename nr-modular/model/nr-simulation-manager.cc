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
#include "ns3/config.h"

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
      m_networkManager(nullptr),
      m_trafficManager(nullptr),
      m_metricsManager(nullptr),
      m_configManager(nullptr),
      m_outputManager(nullptr)
    //   m_bwpManager(nullptr)
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
    m_networkManager = nullptr;
    
    // Cleanup MILP components
    m_bwpManager = nullptr;
    m_milpInterface = nullptr;
    m_milpScheduler = nullptr;
    
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
    m_networkManager = CreateObject<NrNetworkManager>();
    m_channelManager = CreateObject<NrChannelManager>();
    m_mobilityManager = CreateObject<NrMobilityManager>();
    m_trafficManager = CreateObject<NrTrafficManager>();
    m_metricsManager = CreateObject<NrMetricsManager>();
    m_outputManager = CreateObject<NrOutputManager>();
    
    // MILP Components (created but configured later if enabled)
    m_bwpManager = CreateObject<NrBwpManager>();
    m_milpInterface = CreateObject<NrMilpInterface>();
    m_milpScheduler = nullptr;  // Created during SetupMilpScheduler()
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
    m_wallClockStart = start;
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
    m_networkManager->SetConfig(m_config);  // NEW!
    m_trafficManager->SetConfig(m_config);
    m_metricsManager->SetConfig(m_config);
    m_outputManager->SetConfig(m_config);
    // m_bwpManager->SetConfig(m_config);
    // m_bwpManager->SetNetworkManager(m_networkManager); 

    // std::cout << "[nr-simulation-manager] m_bwpManager is " << (m_bwpManager ? "valid" : "nullptr") << std::endl;
    // std::cout << "[nr-simulation-manager] m_bwpManager->IsEnabled() is " << (m_bwpManager && m_bwpManager->IsEnabled() ? "true" : "false") << std::endl;
    
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

    // =================================================================
    // STEP 8c: Setup MILP Scheduler (if enabled)
    // =================================================================
    std::cout << "Step 8c/10: Setting up MILP scheduler (if enabled)..." << std::endl;
    SetupMilpScheduler();

    

    
    // STEP 8: Assign IP Addresses
    std::cout << "Step 8/10: Assigning IP addresses and attaching UEs..." << std::endl;
    m_networkManager->AssignIpAddresses(ueNodes);

    std::cout << "\nEnabling handover tracking..." << std::endl;
    m_networkManager->EnableHandoverTracing(true);
    // ⭐ NEW: Enable handover tracking (if you have multiple gNBs)
    // if (m_config->topology.gnbCount > 1)
    // {
    //     std::cout << "\nEnabling handover tracking..." << std::endl;
    //     m_networkManager->EnableHandoverTracing(true);
    // }
    
    // NEW STEP: Now attach after positions are set
    std::cout << "Step 8b/10: Attaching UEs to closest gNBs..." << std::endl;
    m_networkManager->AttachUes(
        m_networkManager->GetNrHelper(),
        m_networkManager->GetUeDevices(),
        m_networkManager->GetGnbDevices()
    );

    
    // STEP 8d: Link MILP data to the LIVE gNB Schedulers
    std::cout << "Linking MILP data to gNB schedulers..." << std::endl;
    for (uint32_t i = 0; i < m_networkManager->GetGnbDevices().GetN(); ++i) {
        Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(m_networkManager->GetGnbDevices().Get(i));
        
        // Correct way to get the scheduler: 
        // NrGnbNetDevice -> GetMac(bwpId) -> GetScheduler()
        Ptr<NrMacScheduler> baseSched = gnbDev->GetScheduler(0);
        Ptr<NrMilpExecutorScheduler> milpSched = DynamicCast<NrMilpExecutorScheduler>(baseSched);
        
        if (milpSched) {
            milpSched->SetBwpManager(m_bwpManager);
            milpSched->Initialize(m_networkManager);
            m_milpScheduler = milpSched; 
            std::cout << "  ✓ Linked MILP data to gNB " << i << std::endl;
        } else {
            std::cout << "  ⚠ WARNING: gNB " << i << " is not using NrMilpExecutorScheduler!" << std::endl;
        }
    }

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
        m_metricsManager,
        m_channelManager,
        m_mobilityManager
        // m_bwpManager
    );

    m_outputManager->InitializeTelemetry();

    // Configure for UDP publishing
    m_outputManager->ConfigurePublishing(
        NrOutputManager::PUBLISH_UDP,
        "127.0.0.1",
        5555
    );

    // Start with 100ms updates
    m_outputManager->StartTelemetry(m_config->monitoring.monitorInterval);

    // =================================================================
    // // NEW: ENABLE BWP EXTERNAL CONTROL
    // // =================================================================
    // // We check the configuration and enable the control port (e.g., 5556)
    // if (m_config->monitoring.enableExternalControl) 
    // {
    //     std::cout << "Starting BWP External Control Server..." << std::endl;
    //     // This starts the HandleControlPacket loop and SyncAndApplyAllocations loop
    //     m_bwpManager->EnableExternalControl(5556); 
    // }
    
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

    // Total time
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(end - m_wallClockStart).count();
    NS_LOG_INFO("Total Simulation Time (including finalization): " << totalDuration << " seconds");
    std::cout << "Total Simulation Time (including finalization): " << totalDuration << " seconds" << std::endl;
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

// Ptr<NrBwpManager>
// NrSimulationManager::GetBwpManager() const
// {
//     return m_bwpManager;
// } 

Ptr<NrHelper>
NrSimulationManager::GetNrHelper() const
{
    if (m_networkManager)
    {        return m_networkManager->GetNrHelper();
    }
    return nullptr;
}

Ptr<NrSimConfig>
NrSimulationManager::GetConfig() const
{
    return m_config;
}

Ptr<NrBwpManager>
NrSimulationManager::GetBwpManager() const
{
    return m_bwpManager;
}

// ============================================================================
// MILP SCHEDULER SETUP
// ============================================================================

// void
// NrSimulationManager::SetupMilpScheduler()
// {
//     NS_LOG_FUNCTION(this);
    
//     /*
//      * ========================================================================
//      * MILP SCHEDULER SETUP - FULL IMPLEMENTATION
//      * ========================================================================
//      * 
//      * This method sets up the MILP-based optimal scheduler (Baseline 2).
//      * 
//      * Architecture:
//      * 1. Check if MILP scheduling is enabled
//      * 2. Build MILP problem from simulation parameters
//      * 3. Solve MILP problem (STUB: round-robin for now)
//      * 4. Load solution into BWP Manager
//      * 5. Create MILP Executor Scheduler
//      * 6. Initialize scheduler with RNTI mappings
//      * 7. Install scheduler on all gNBs
//      * 
//      * Current Implementation:
//      * - Uses round-robin allocation (STUB for testing)
//      * - TODO: Replace with actual Python MILP solver call
//      */
    
//     // ========================================================================
//     // Step 1: Check if MILP Scheduling is Enabled
//     // ========================================================================
    
//     // TODO: Add scheduling mode to config
//     // For now, always enable MILP scheduling
//     bool useMilpScheduling = true;
    
//     if (!useMilpScheduling)
//     {
//         NS_LOG_INFO("MILP scheduling disabled in configuration");
//         std::cout << "  MILP scheduling: Disabled (using default scheduler)" << std::endl;
//         return;
//     }
    
//     NS_LOG_INFO("MILP scheduling enabled - configuring...");
//     std::cout << "  MILP scheduling: Enabled" << std::endl;
    
//     // ========================================================================
//     // Step 2: Build MILP Problem from Configuration
//     // ========================================================================
    
//     std::cout << "  Building MILP problem..." << std::endl;
    
//     // Get simulation parameters
//     uint32_t numUes = m_config->topology.ueCount;
//     double simDuration = m_config->simDuration;
    
//     // Calculate number of slots
//     // Numerology μ=1 → slot duration = 0.5ms → 2000 slots per second
//     // uint32_t numSlots = static_cast<uint32_t>(simDuration * 2000);
//     // Add a 10-slot buffer for scheduling lookahead
//     uint32_t numSlots = static_cast<uint32_t>(simDuration * 2000) + 10;
    
//     // Get bandwidth and calculate PRBs
//     // TODO: Get from actual BWP configuration
//     // For now: Assume 100 MHz bandwidth → 273 PRBs
//     uint32_t totalPrbs = 273;
    
//     NS_LOG_INFO("MILP Problem Parameters:");
//     NS_LOG_INFO("  Number of UEs: " << numUes);
//     NS_LOG_INFO("  Number of slots: " << numSlots);
//     NS_LOG_INFO("  Total PRBs: " << totalPrbs);
//     NS_LOG_INFO("  Simulation duration: " << simDuration << " seconds");
    
//     std::cout << "    UEs: " << numUes << std::endl;
//     std::cout << "    Slots: " << numSlots 
//               << " (" << simDuration << "s × 2000 slots/s)" << std::endl;
//     std::cout << "    PRBs: " << totalPrbs << std::endl;
    
//     // Build MILP problem structure (field names from nr-milp-types.h)
//     MilpProblem problem;
//     problem.totalSlots         = numSlots;
//     problem.numUEs             = numUes;
//     problem.totalBandwidthPrbs = totalPrbs;
//     problem.bandwidth          = 100e6;
//     problem.timeWindow         = simDuration;
//     problem.numerology         = 1;
//     problem.slotDuration       = 0.0005;
    
//     // Add UE SLAs (field names from UeSla struct in nr-milp-types.h)
//     for (uint32_t ueId = 0; ueId < numUes; ueId++)
//     {
//         UeSla sla;
//         sla.ueId           = ueId;
//         sla.sliceType      = SliceType::eMBB;  // eMBB not EMBB
//         sla.throughputMbps = 10.0;              // not minThroughputMbps
//         sla.latencyMs      = 100.0;             // not maxLatencyMs
//         sla.mcs            = 16;                // not fixedMcs
//         sla.tbs            = 0;                 // calculated from MCS table
//         // Note: UeSla has no packetSize field
        
//         problem.ues.push_back(sla);             // not ueSlas
//     }
    
//     NS_LOG_INFO("Added " << problem.ues.size() << " UE SLAs to MILP problem");
    
//     // ========================================================================
//     // Step 3: Solve MILP Problem
//     // ========================================================================
    
//     std::cout << "  Solving MILP problem..." << std::endl;
    
//     /*
//      * PRODUCTION VERSION (when Python solver is ready):
//      * 
//      * std::cout << "    Connecting to MILP solver..." << std::endl;
//      * MilpSolution solution;
//      * bool solved = m_milpInterface->SolveProblem(problem, solution);
//      * 
//      * if (!solved)
//      * {
//      *     NS_FATAL_ERROR("MILP solver failed to find solution!");
//      * }
//      * 
//      * std::cout << "    ✅ MILP problem solved!" << std::endl;
//      * std::cout << "    Objective value: " << solution.objectiveValue << std::endl;
//      */
    
//     // STUB VERSION: Generate round-robin allocation for testing
//     std::cout << "    Using STUB solver (round-robin allocation)" << std::endl;
//     std::cout << "    TODO: Replace with Python MILP solver" << std::endl;
    
//     // MilpSolution fields from nr-milp-types.h:
//     // - status (string), objectiveValue (double), solveTimeSeconds, allocations, summary
//     MilpSolution solution;
//     solution.status         = "optimal";   // not solved bool
//     solution.objectiveValue = 0.0;           // not objective
//     solution.solveTimeSeconds = 0.0;
    
//     // Simple round-robin: divide PRBs equally among UEs
//     uint32_t prbsPerUe = totalPrbs / numUes;
//     uint32_t remainderPrbs = totalPrbs % numUes;
    
//     std::cout << "    Allocating ~" << prbsPerUe << " PRBs per UE" << std::endl;
    
//     for (uint32_t slot = 0; slot < numSlots; slot++)
//     {
//         uint32_t currentStartPrb = 0;
        
//         for (uint32_t ueId = 0; ueId < numUes; ueId++)
//         {
//             PrbAllocation alloc;
//             alloc.ueId = ueId;
//             alloc.slotId = slot;
//             alloc.startPrb = currentStartPrb;
            
//             // Last UE gets any remainder PRBs
//             if (ueId == numUes - 1)
//             {
//                 alloc.numPrbs = prbsPerUe + remainderPrbs;
//             }
//             else
//             {
//                 alloc.numPrbs = prbsPerUe;
//             }
            
//             solution.allocations.push_back(alloc);
//             currentStartPrb += alloc.numPrbs;
//         }
//     }
    
//     NS_LOG_INFO("Generated " << solution.allocations.size() 
//                 << " allocations (round-robin)");
//     std::cout << "    Generated " << solution.allocations.size() 
//               << " allocations" << std::endl;
    
//     // ========================================================================
//     // Step 4: Load Solution into BWP Manager
//     // ========================================================================
//     std::cout << "  Loading solution into BWP Manager..." << std::endl;
//     GetNrHelper()->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMilpExecutorScheduler"));

//     // Ensure numSlots is calculated with the buffer as you did:
//     // uint32_t numSlots = static_cast<uint32_t>(simDuration * 2000) + 10; // Add a 10-slot buffer for scheduling lookahead

//     std::cout << "  Loading solution into BWP Manager..." << std::endl;
    
//     m_bwpManager->LoadMilpSolution(solution);
    
//     // Verify loading with statistics
//     NrBwpManager::Statistics stats = m_bwpManager->GetStatistics();
    
//     // Statistics field names from NrBwpManager::Statistics in nr-bwp-manager.h:
//     // numActiveSlots (not allocatedSlots), avgPrbsPerActiveSlot (not avgPrbsPerUe)
//     NS_LOG_INFO("BWP Manager Statistics:");
//     NS_LOG_INFO("  Total allocations: " << stats.totalAllocations);
//     NS_LOG_INFO("  Active slots: " << stats.numActiveSlots);
//     NS_LOG_INFO("  Avg PRBs/active slot: " << stats.avgPrbsPerActiveSlot);
    
//     std::cout << "    Total allocations: " << stats.totalAllocations << std::endl;
//     std::cout << "    Slots with allocations: " << stats.numActiveSlots
//               << " / " << numSlots << std::endl;
//     std::cout << "    Avg PRBs per active slot: " << stats.avgPrbsPerActiveSlot << std::endl;
    
//     // Sanity check
//     if (stats.totalAllocations != solution.allocations.size())
//     {
//         NS_LOG_WARN("Allocation count mismatch! Expected " 
//                     << solution.allocations.size() 
//                     << " but BWP Manager has " << stats.totalAllocations);
//     }
    
//     // ========================================================================
//     // Step 5: Create MILP Executor Scheduler
//     // ========================================================================
    
//     std::cout << "  Creating MILP Executor Scheduler..." << std::endl;
    
//     m_milpScheduler = CreateObject<NrMilpExecutorScheduler>();
    
//     if (!m_milpScheduler)
//     {
//         NS_FATAL_ERROR("Failed to create MILP Executor Scheduler!");
//     }
    
//     // Set BWP Manager reference
//     m_milpScheduler->SetBwpManager(m_bwpManager);
    
//     NS_LOG_INFO("MILP Executor Scheduler created");
    
//     // ========================================================================
//     // Step 6: Initialize Scheduler with RNTI Mappings
//     // ========================================================================
    
//     std::cout << "  Initializing scheduler with RNTI mappings..." << std::endl;
    
//     // This caches the ueId ↔ RNTI mapping for fast lookup
//     m_milpScheduler->Initialize(m_networkManager);
    
//     NS_LOG_INFO("Scheduler initialized with RNTI mappings");
//     std::cout << "    Mapped " << numUes << " UEs to RNTIs" << std::endl;
    
//     // ========================================================================
//     // Step 7: Install Scheduler on gNBs
//     // ========================================================================
    
//     std::cout << "  Installing scheduler on gNBs..." << std::endl;
    
//     NetDeviceContainer gnbDevices = m_networkManager->GetGnbDevices();
//     uint32_t gnbCount = gnbDevices.GetN();
    
//     for (uint32_t i = 0; i < gnbCount; i++)
//     {
//         Ptr<NetDevice> netDev = gnbDevices.Get(i);
//         Ptr<NrGnbNetDevice> gnbNetDev = DynamicCast<NrGnbNetDevice>(netDev);
        
//         if (!gnbNetDev)
//         {
//             NS_LOG_WARN("Device " << i << " is not an NrGnbNetDevice");
//             continue;
//         }
        
//         /*
//          * NOTE: The NR scheduler cannot be swapped post-installation via
//          * direct API. The correct way to use a custom scheduler in NR is:
//          *
//          *   nrHelper->SetSchedulerTypeId(
//          *       TypeId::LookupByName("ns3::NrMilpExecutorScheduler"));
//          *
//          * called on NrHelper BEFORE InstallGnbDevice().
//          *
//          * For this project, call the following in nr-network-manager.cc
//          * inside SetupNrInfrastructure(), before device installation:
//          *
//          *   m_nrHelper->SetSchedulerTypeId(
//          *       TypeId::LookupByName("ns3::NrMilpExecutorScheduler"));
//          *
//          * The BWP Manager (m_bwpManager) and RNTI map are already set up
//          * above and will be injected via SetBwpManager() after this function
//          * returns through the scheduler's own Initialize() call above.
//          *
//          * TODO: Move scheduler TypeId configuration to nr-network-manager.cc
//          */
//         NS_LOG_INFO("gNB " << i << ": scheduler type must be set pre-installation via NrHelper");
//     }
    
//     std::cout << "    Installed on " << gnbCount << " gNB(s)" << std::endl;
    
//     // ========================================================================
//     // DONE
//     // ========================================================================
    
//     std::cout << "  ✅ MILP scheduler setup complete!" << std::endl;
//     std::cout << std::endl;
    
//     NS_LOG_INFO("MILP scheduler setup complete - ready for simulation");
    
//     // Print configuration summary
//     std::cout << "  MILP Scheduler Summary:" << std::endl;
//     std::cout << "    Mode: Blind Executor (Baseline 2)" << std::endl;
//     std::cout << "    Allocations: " << stats.totalAllocations << std::endl;
//     std::cout << "    Time horizon: " << numSlots << " slots (" 
//               << simDuration << " seconds)" << std::endl;
//     std::cout << "    Frequency: " << totalPrbs << " PRBs" << std::endl;
//     std::cout << std::endl;
// }

void
NrSimulationManager::SetupMilpScheduler()
{
    NS_LOG_FUNCTION(this);
    
    // We can no longer set the SchedulerTypeId here because m_networkManager->GetNrHelper() 
    // is null at this stage of initialization. We will move that call to Initialize().

    std::cout << "  Building MILP problem..." << std::endl;
    
    uint32_t numUes = m_config->topology.ueCount;
    double simDuration = m_config->simDuration;
    
    uint32_t numSlots = static_cast<uint32_t>(simDuration * 2000) + 10;
    uint32_t totalPrbs = 273; 
    
    MilpProblem problem;
    problem.totalSlots = numSlots;
    problem.numUEs = numUes;
    problem.totalBandwidthPrbs = totalPrbs;
    problem.numerology = 1;
    
    for (uint32_t ueId = 0; ueId < numUes; ueId++)
    {
        UeSla sla;
        sla.ueId = ueId;
        sla.sliceType = SliceType::eMBB;
        sla.throughputMbps = 10.0;
        sla.mcs = 16;
        problem.ues.push_back(sla);
    }
    
    std::cout << "  Solving MILP problem (Stub)..." << std::endl;
    MilpSolution solution;
    solution.status = "optimal";
    
    uint32_t prbsPerUe = totalPrbs / numUes;
    for (uint32_t slot = 0; slot < numSlots; slot++)
    {
        uint32_t currentStartPrb = 0;
        for (uint32_t ueId = 0; ueId < numUes; ueId++)
        {
            PrbAllocation alloc;
            alloc.ueId = ueId;
            alloc.slotId = slot;
            alloc.startPrb = currentStartPrb;
            alloc.numPrbs = (ueId == numUes - 1) ? (totalPrbs - currentStartPrb) : prbsPerUe;
            solution.allocations.push_back(alloc);
            currentStartPrb += alloc.numPrbs;
        }
    }
    
    std::cout << "  Loading solution into BWP Manager..." << std::endl;
    m_bwpManager->LoadMilpSolution(solution);
    
    // m_milpScheduler = CreateObject<NrMilpExecutorScheduler>();
    // m_milpScheduler->SetBwpManager(m_bwpManager);
    
    std::cout << "  ✅ MILP data structures prepared!" << std::endl;
}

} // namespace ns3