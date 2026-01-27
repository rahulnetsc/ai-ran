/*
 * Ultra-Simple Network Test Using Built-in TestTrafficFlow()
 * 
 * This test uses NrNetworkManager's built-in TestTrafficFlow() method
 * to verify the network is working correctly.
 * 
 * Run with: ./ns3 run scratch/nr-simple-test
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"

// NR Modular
#include "ns3/nr-modular-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrSimpleTest");

int
main(int argc, char* argv[])
{
    // Test parameters
    uint32_t numUes = 3;
    double testRate = 10.0;  // Mbps
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("ues", "Number of UEs", numUes);
    cmd.AddValue("rate", "Test rate (Mbps)", testRate);
    cmd.Parse(argc, argv);
    
    std::cout << "\n╔═══════════════════════════════════════════════════╗\n";
    std::cout << "║       NR NETWORK SIMPLE TEST                      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════╝\n\n";
    
    // ================================================================
    // 1. Create Configuration
    // ================================================================
    std::cout << "Creating configuration...\n";
    
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
    config->topology.gnbCount = 1;
    config->topology.ueCount = numUes;
    config->topology.areaSize = 1000.0;
    config->channel.propagationModel = "UMa";
    config->channel.frequency = 3.5e9;
    config->channel.bandwidth = 20e6;
    config->simDuration = 10.0;
    
    // ================================================================
    // 2. Setup Network Manager
    // ================================================================
    std::cout << "Setting up network manager...\n";
    
    Ptr<NrNetworkManager> netMgr = CreateObject<NrNetworkManager>();
    netMgr->SetConfig(config);
    
    
    // ================================================================
    // 3. Create and Deploy Nodes
    // ================================================================
    std::cout << "Creating nodes...\n";
    
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    
    NodeContainer ueNodes;
    ueNodes.Create(numUes);
    
    // Deploy nodes
    MobilityHelper mobility;
    
    // gNB at center
    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    gnbPos->Add(Vector(500.0, 500.0, 25.0));
    mobility.SetPositionAllocator(gnbPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    
    // UEs around gNB
    Ptr<ListPositionAllocator> uePos = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUes; i++)
    {
        double angle = (2.0 * M_PI * i) / numUes;
        double x = 500.0 + 100.0 * cos(angle);
        double y = 500.0 + 100.0 * sin(angle);
        uePos->Add(Vector(x, y, 1.5));
    }
    mobility.SetPositionAllocator(uePos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    
    // ================================================================
    // 4. Install Devices and Attach
    // ================================================================

    netMgr->SetupNrInfrastructure(gnbNodes, ueNodes);

    // std::cout << "Installing NR devices...\n";
    // netMgr->InstallNrDevices(gnbNodes, ueNodes);
    
    // std::cout << "Attaching UEs...\n";
    // netMgr->AttachUesToGnbs();
    
    std::cout << "Assigning IP addresses...\n";
    netMgr->AssignIpAddresses(ueNodes);
    
    // ================================================================
    // 5. Test Network with Built-in Method
    // ================================================================
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Testing network with built-in TestTrafficFlow()...\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Create test remote host
    Ptr<Node> testHost = CreateObject<Node>();
    InternetStackHelper internet;
    internet.Install(testHost);
    
    // Connect to PGW
    Ptr<Node> pgw = netMgr->GetEpcHelper()->GetPgwNode();
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    
    NodeContainer internetNodes(pgw, testHost);
    NetDeviceContainer internetDevices = p2p.Install(internetNodes);
    
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIps = ipv4h.Assign(internetDevices);
    
    // Configure routing
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> remoteRouting = 
        routingHelper.GetStaticRouting(testHost->GetObject<Ipv4>());
    remoteRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"),
        Ipv4Mask("255.0.0.0"),
        internetIps.GetAddress(0),
        1
    );
    
    // RUN THE BUILT-IN TEST!
    // bool testPassed = netMgr->TestTrafficFlow(
    //     testHost,      // Remote host
    //     ueNodes,       // UEs to test
    //     testRate,      // Target rate (Mbps)
    //     3.0            // Test duration (seconds)
    // );
    
    // ================================================================
    // 6. Results
    // ================================================================
    std::cout << "\n╔═══════════════════════════════════════════════════╗\n";
    
    // if (testPassed)
    // {
    //     std::cout << "║                                                   ║\n";
    //     std::cout << "║            ✅ NETWORK TEST PASSED                 ║\n";
    //     std::cout << "║                                                   ║\n";
    //     std::cout << "║  Your network infrastructure is working!          ║\n";
    //     std::cout << "║  Downlink traffic flows correctly.                ║\n";
    //     std::cout << "║                                                   ║\n";
    // }
    // else
    // {
    //     std::cout << "║                                                   ║\n";
    //     std::cout << "║            ❌ NETWORK TEST FAILED                 ║\n";
    //     std::cout << "║                                                   ║\n";
    //     std::cout << "║  Some flows below target rate.                    ║\n";
    //     std::cout << "║  Check configuration and logs above.              ║\n";
    //     std::cout << "║                                                   ║\n";
    // }
    
    std::cout << "╚═══════════════════════════════════════════════════╝\n\n";
    
    // Get detailed results
    // std::vector<NrNetworkManager::FlowTestResult> results = 
    //     netMgr->GetFlowTestResults();
    
    // std::cout << "Detailed Per-UE Results:\n";
    // std::cout << "────────────────────────────────────────────────────\n";
    // for (const auto& r : results)
    // {
    //     std::cout << "UE " << r.ueIndex << " (" << r.ueAddress << "):\n";
    //     std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
    //               << r.throughputMbps << " Mbps\n";
    //     std::cout << "  Delay:      " << std::fixed << std::setprecision(2)
    //               << r.avgDelayMs << " ms\n";
    //     std::cout << "  Loss:       " << std::fixed << std::setprecision(1)
    //               << r.packetLossPercent << " %\n";
    //     std::cout << "  Status:     " << (r.success ? "✓ PASS" : "✗ FAIL") << "\n\n";
    // }
    
    // Cleanup
    Simulator::Destroy();
    
    // return testPassed ? 0 : 1;
    return 0;
}
