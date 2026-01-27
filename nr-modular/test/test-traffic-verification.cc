/*
 * Copyright (c) 2026 ARTPARK
 * 
 * CORRECTED End-to-end test with TRAFFIC VERIFICATION
 * Now includes proper NR network setup (EPC, devices, attachment, IPs)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-modular-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-sink.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <chrono>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestTrafficVerificationCorrected");

// ===================================================================
// TIMING HELPER
// ===================================================================
void
PrintStepTime(const std::string& stepName, 
              std::chrono::high_resolution_clock::time_point& lastStepTime)
{
    auto now = std::chrono::high_resolution_clock::now();
    auto stepDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStepTime);
    
    std::cout << "  [Completed in " << std::fixed << std::setprecision(2) 
              << (stepDuration.count() / 1000.0) << "s]\n\n";
    
    lastStepTime = now;  // Update for next step
}

// ===================================================================
// POSITION TRACKING
// ===================================================================
void
TrackPositions(NodeContainer ueNodes, uint32_t numWaypointUes, std::ofstream& posFile)
{
    for (uint32_t i = 0; i < numWaypointUes && i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mob = ueNodes.Get(i)->GetObject<MobilityModel>();
        if (mob)
        {
            Vector pos = mob->GetPosition();
            posFile << Simulator::Now().GetSeconds() << ","
                    << i << ","
                    << pos.x << ","
                    << pos.y << ","
                    << pos.z << std::endl;
        }
    }
}

// ===================================================================
// UE-GNB PROXIMITY ANALYSIS
// ===================================================================
void
PrintUeGnbAssociations(NodeContainer gnbNodes, NodeContainer ueNodes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           UE-gNB PROXIMITY ANALYSIS                       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::map<uint32_t, uint32_t> gnbUeCount;
    
    for (uint32_t ueId = 0; ueId < ueNodes.GetN(); ++ueId)
    {
        Ptr<MobilityModel> ueMob = ueNodes.Get(ueId)->GetObject<MobilityModel>();
        if (!ueMob) continue;
        
        Vector uePos = ueMob->GetPosition();
        
        double minDist = std::numeric_limits<double>::max();
        uint32_t nearestGnb = 0;
        
        for (uint32_t gnbId = 0; gnbId < gnbNodes.GetN(); ++gnbId)
        {
            Ptr<MobilityModel> gnbMob = gnbNodes.Get(gnbId)->GetObject<MobilityModel>();
            if (!gnbMob) continue;
            
            Vector gnbPos = gnbMob->GetPosition();
            double dx = uePos.x - gnbPos.x;
            double dy = uePos.y - gnbPos.y;
            double dz = uePos.z - gnbPos.z;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist < minDist)
            {
                minDist = dist;
                nearestGnb = gnbId;
            }
        }
        
        gnbUeCount[nearestGnb]++;
        
        std::cout << "UE " << std::setw(2) << ueId << ": ";
        std::cout << "Nearest gNB " << nearestGnb << " ";
        std::cout << "(distance: " << std::fixed << std::setprecision(1) 
                  << minDist << " m)" << std::endl;
    }
    
    std::cout << "\n--- gNB Load Distribution ---\n";
    for (uint32_t gnbId = 0; gnbId < gnbNodes.GetN(); ++gnbId)
    {
        uint32_t count = gnbUeCount[gnbId];
        std::cout << "gNB " << gnbId << ": " << count << " UEs [";
        for (uint32_t i = 0; i < count; ++i) std::cout << "█";
        std::cout << "]" << std::endl;
    }
    std::cout << std::endl;
}

// ===================================================================
// PACKET RECEPTION VERIFICATION
// ===================================================================
void
VerifyPacketReception(ApplicationContainer serverApps, uint32_t numUes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           PACKET RECEPTION VERIFICATION                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    uint32_t totalSinks = serverApps.GetN();
    uint32_t expectedSinks = numUes * 2;  // DL + UL per UE
    
    std::cout << "Total sinks: " << totalSinks << " (expected: " << expectedSinks << ")\n\n";
    
    uint32_t dlSinksWithData = 0;
    uint32_t ulSinksWithData = 0;
    uint64_t totalDlBytes = 0;
    uint64_t totalUlBytes = 0;
    
    std::cout << "--- Downlink Traffic (Remote → UEs) ---\n";
    for (uint32_t i = 0; i < numUes; ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(i * 2));
        if (sink)
        {
            uint64_t bytes = sink->GetTotalRx();
            totalDlBytes += bytes;
            
            std::cout << "  UE " << std::setw(2) << i << ": ";
            std::cout << std::setw(10) << bytes << " bytes ";
            
            if (bytes > 0)
            {
                dlSinksWithData++;
                std::cout << "(" << std::fixed << std::setprecision(2) 
                          << (bytes / 1024.0 / 1024.0) << " MB) ✓";
            }
            else
            {
                std::cout << "✗ NO DATA!";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n--- Uplink Traffic (UEs → Remote) ---\n";
    for (uint32_t i = 0; i < numUes; ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(i * 2 + 1));
        if (sink)
        {
            uint64_t bytes = sink->GetTotalRx();
            totalUlBytes += bytes;
            
            std::cout << "  UE " << std::setw(2) << i << ": ";
            std::cout << std::setw(10) << bytes << " bytes ";
            
            if (bytes > 0)
            {
                ulSinksWithData++;
                std::cout << "(" << std::fixed << std::setprecision(2) 
                          << (bytes / 1024.0 / 1024.0) << " MB) ✓";
            }
            else
            {
                std::cout << "✗ NO DATA!";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n--- Summary ---\n";
    std::cout << "DL: " << dlSinksWithData << "/" << numUes << " UEs received data ";
    std::cout << "(Total: " << std::fixed << std::setprecision(2) 
              << (totalDlBytes / 1024.0 / 1024.0) << " MB)";
    if (dlSinksWithData == numUes) std::cout << " ✓";
    else std::cout << " ✗";
    std::cout << std::endl;
    
    std::cout << "UL: " << ulSinksWithData << "/" << numUes << " UEs sent data ";
    std::cout << "(Total: " << (totalUlBytes / 1024.0 / 1024.0) << " MB)";
    if (ulSinksWithData == numUes) std::cout << " ✓";
    else std::cout << " ✗";
    std::cout << std::endl;
    
    if (dlSinksWithData < numUes || ulSinksWithData < numUes)
    {
        std::cout << "\n⚠️  WARNING: Some UEs have no traffic!\n";
        std::cout << "    Possible causes:\n";
        std::cout << "    - Routing issue\n";
        std::cout << "    - IP address mismatch\n";
        std::cout << "    - Application not started\n";
    }
    else
    {
        std::cout << "\n✓ SUCCESS: All UEs exchanged traffic!\n";
    }
    std::cout << std::endl;
}

// ===================================================================
// FLOWMONITOR ANALYSIS
// ===================================================================
void
AnalyzeFlowMonitor(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, uint32_t numUes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           FLOWMONITOR ANALYSIS                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    std::cout << "Total flows detected: " << stats.size() << "\n\n";
    
    struct UeStats
    {
        uint64_t txBytes = 0;
        uint64_t rxBytes = 0;
        uint32_t txPackets = 0;
        uint32_t rxPackets = 0;
        double throughputMbps = 0.0;
        double avgDelayMs = 0.0;
        double lossPercent = 0.0;
    };
    
    std::map<std::string, UeStats> ueFlows;  // "UE_X_DL" or "UE_X_UL"
    
    // Parse flows
    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        
        std::string direction = "";
        uint32_t ueId = 0;
        
        // Determine direction based on port
        if (t.destinationPort >= 10000 && t.destinationPort < 11000)
        {
            direction = "DL";
            ueId = t.destinationPort - 10000;
        }
        else if (t.destinationPort >= 20000 && t.destinationPort < 21000)
        {
            direction = "UL";
            ueId = t.destinationPort - 20000;
        }
        else
        {
            continue;  // Ignore other flows
        }
        
        if (ueId >= numUes) continue;
        
        std::string key = "UE_" + std::to_string(ueId) + "_" + direction;
        
        UeStats& s = ueFlows[key];
        s.txBytes = flow.second.txBytes;
        s.rxBytes = flow.second.rxBytes;
        s.txPackets = flow.second.txPackets;
        s.rxPackets = flow.second.rxPackets;
        
        if (s.rxPackets > 0)
        {
            s.throughputMbps = (s.rxBytes * 8.0) / flow.second.timeLastRxPacket.GetSeconds() / 1e6;
            s.avgDelayMs = (flow.second.delaySum.GetMilliSeconds() / s.rxPackets);
        }
        
        if (s.txPackets > 0)
        {
            uint32_t lostPackets = s.txPackets - s.rxPackets;
            s.lossPercent = (lostPackets * 100.0) / s.txPackets;
        }
    }
    
    // Print table
    std::cout << "UE | DL (Mbps) | UL (Mbps) | DL Delay | UL Delay | DL Loss | UL Loss\n";
    std::cout << "---+-----------+-----------+----------+----------+---------+---------\n";
    
    uint32_t uesWithDl = 0;
    uint32_t uesWithUl = 0;
    double totalDlThroughput = 0.0;
    double totalUlThroughput = 0.0;
    
    for (uint32_t i = 0; i < numUes; ++i)
    {
        std::string dlKey = "UE_" + std::to_string(i) + "_DL";
        std::string ulKey = "UE_" + std::to_string(i) + "_UL";
        
        auto dlIt = ueFlows.find(dlKey);
        auto ulIt = ueFlows.find(ulKey);
        
        bool hasDl = (dlIt != ueFlows.end() && dlIt->second.rxPackets > 0);
        bool hasUl = (ulIt != ueFlows.end() && ulIt->second.rxPackets > 0);
        
        if (hasDl)
        {
            uesWithDl++;
            totalDlThroughput += dlIt->second.throughputMbps;
        }
        if (hasUl)
        {
            uesWithUl++;
            totalUlThroughput += ulIt->second.throughputMbps;
        }
        
        std::cout << std::setw(2) << i << " | ";
        std::cout << std::fixed << std::setprecision(2);
        
        // DL throughput
        std::cout << std::setw(9) << (hasDl ? dlIt->second.throughputMbps : 0.0) << " | ";
        
        // UL throughput
        std::cout << std::setw(9) << (hasUl ? ulIt->second.throughputMbps : 0.0) << " | ";
        
        // DL delay
        std::cout << std::setw(7) << (hasDl ? dlIt->second.avgDelayMs : 0.0) << "ms | ";
        
        // UL delay
        std::cout << std::setw(7) << (hasUl ? ulIt->second.avgDelayMs : 0.0) << "ms | ";
        
        // DL loss
        std::cout << std::setw(6) << (hasDl ? dlIt->second.lossPercent : 0.0) << "% | ";
        
        // UL loss
        std::cout << std::setw(6) << (hasUl ? ulIt->second.lossPercent : 0.0) << "%";
        
        if (!hasDl || !hasUl) std::cout << " ⚠️";
        std::cout << std::endl;
    }
    
    std::cout << "\n--- Aggregate Statistics ---\n";
    std::cout << "DL: " << uesWithDl << "/" << numUes << " UEs, ";
    std::cout << "Total: " << std::fixed << std::setprecision(2) << totalDlThroughput << " Mbps, ";
    std::cout << "Avg: " << (uesWithDl > 0 ? totalDlThroughput / uesWithDl : 0.0) << " Mbps/UE\n";
    
    std::cout << "UL: " << uesWithUl << "/" << numUes << " UEs, ";
    std::cout << "Total: " << totalUlThroughput << " Mbps, ";
    std::cout << "Avg: " << (uesWithUl > 0 ? totalUlThroughput / uesWithUl : 0.0) << " Mbps/UE\n";
    
    if (uesWithDl == numUes && uesWithUl == numUes)
    {
        std::cout << "\n✓ SUCCESS: All UEs have traffic in both directions!\n";
    }
    else
    {
        std::cout << "\n⚠️  WARNING: Some UEs missing traffic\n";
    }
}

// ===================================================================
// MAIN TEST
// ===================================================================
int
main(int argc, char* argv[])
{
    std::string configFile = "input/test-waypoints-config.json";
    
    CommandLine cmd;
    cmd.AddValue("config", "Path to configuration JSON file", configFile);
    cmd.Parse(argc, argv);

    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║      TRAFFIC VERIFICATION TEST (CORRECTED)                ║\n";
    std::cout << "║      With proper NR network setup                         ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

    // =================================================================
    // START TIMER
    // =================================================================
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastStepTime = startTime;  // Track time per step
    
    std::cout << "⏱️  Test execution started at " 
              << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) 
              << "\n\n";

    // =================================================================
    // STEP 1: Load Configuration
    // =================================================================
    std::cout << "Step 1/10: Loading configuration...\n";
    
    Ptr<NrConfigManager> configMgr = CreateObject<NrConfigManager>();
    Ptr<NrSimConfig> config = configMgr->LoadFromFile(configFile);
    configMgr->ValidateOrAbort(config);
    
    std::cout << "✓ Configuration loaded\n";
    PrintStepTime("Step 1", lastStepTime);

    // =================================================================
    // STEP 2: Setup NR Infrastructure (NEW!)
    // =================================================================
    std::cout << "Step 2/10: Setting up NR infrastructure...\n";
    
    Ptr<NrNetworkManager> netMgr = CreateObject<NrNetworkManager>();
    netMgr->SetConfig(config);
    netMgr->SetupNrInfrastructure();  // EPC, helpers, channel, beamforming
    
    std::cout << "✓ NR infrastructure ready\n";
    PrintStepTime("Step 2", lastStepTime);

    // =================================================================
    // STEP 3: Deploy Topology
    // =================================================================
    std::cout << "Step 3/10: Deploying topology...\n";
    
    Ptr<NrTopologyManager> topoMgr = CreateObject<NrTopologyManager>();
    topoMgr->SetConfig(config);
    topoMgr->DeployTopology();
    
    NodeContainer gnbNodes = topoMgr->GetGnbNodes();
    NodeContainer ueNodes = topoMgr->GetUeNodes();
    
    std::cout << "✓ Topology: " << gnbNodes.GetN() << " gNBs, " 
              << ueNodes.GetN() << " UEs\n";
    PrintStepTime("Step 3", lastStepTime);

    PrintUeGnbAssociations(gnbNodes, ueNodes);

    // =================================================================
    // STEP 4: Install NR Devices (NEW!)
    // =================================================================
    std::cout << "Step 4/10: Installing NR devices...\n";
    
    netMgr->InstallNrDevices(gnbNodes, ueNodes);
    
    std::cout << "✓ NR devices installed\n";
    PrintStepTime("Step 4", lastStepTime);

    // =================================================================
    // STEP 5: Attach UEs to gNBs (NEW!)
    // =================================================================
    std::cout << "Step 5/10: Attaching UEs to gNBs...\n";
    
    netMgr->AttachUesToGnbs();
    
    std::cout << "✓ UEs attached\n";
    PrintStepTime("Step 5", lastStepTime);

    // =================================================================
    // STEP 6: Assign IP Addresses (NEW!)
    // =================================================================
    std::cout << "Step 6/10: Assigning IP addresses...\n";
    
    netMgr->AssignIpAddresses(ueNodes);
    
    std::cout << "✓ IP addresses assigned\n";
    PrintStepTime("Step 6", lastStepTime);

    // =================================================================
    // STEP 7: Install Mobility
    // =================================================================
    std::cout << "Step 7/10: Installing mobility...\n";
    
    Ptr<NrMobilityManager> mobMgr = CreateObject<NrMobilityManager>();
    mobMgr->SetConfig(config);
    mobMgr->InstallUeMobility(ueNodes);
    
    std::cout << "✓ Mobility configured\n";
    PrintStepTime("Step 7", lastStepTime);

    // =================================================================
    // STEP 8: Install Traffic (CORRECTED!)
    // =================================================================
    std::cout << "Step 8/10: Installing traffic...\n";
    
    Ptr<NrTrafficManager> trafficMgr = CreateObject<NrTrafficManager>();
    trafficMgr->SetConfig(config);
    trafficMgr->SetNetworkManager(netMgr);  // ← CRITICAL: Pass network manager!
    trafficMgr->InstallTraffic(gnbNodes, ueNodes);
    
    ApplicationContainer serverApps = trafficMgr->GetServerApps();
    ApplicationContainer clientApps = trafficMgr->GetClientApps();
    
    std::cout << "✓ Traffic: " << serverApps.GetN() << " servers, "
              << clientApps.GetN() << " clients\n";
    PrintStepTime("Step 8", lastStepTime);

    // =================================================================
    // STEP 9: Enable FlowMonitor
    // =================================================================
    std::cout << "Step 9/10: Enabling FlowMonitor...\n";
    
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(
        flowHelper.GetClassifier());
    
    std::cout << "✓ FlowMonitor enabled\n";
    PrintStepTime("Step 9", lastStepTime);

    // =================================================================
    // Position Tracking
    // =================================================================
    std::ofstream posFile("output/ue_positions.csv");
    posFile << "time,ue_id,x,y,z\n";
    
    uint32_t numWaypointUes = config->mobility.ueWaypoints.size();
    
    for (double t = 0.0; t <= config->simDuration; t += 1.0)
    {
        Simulator::Schedule(Seconds(t),
                          &TrackPositions,
                          ueNodes,
                          numWaypointUes,
                          std::ref(posFile));
    }

    // =================================================================
    // Schedule Traffic Verification
    // =================================================================
    Simulator::Schedule(Seconds(config->simDuration - 0.5),
                       &VerifyPacketReception,
                       serverApps,
                       ueNodes.GetN());

    // =================================================================
    // STEP 10: Run Simulation
    // =================================================================
    std::cout << "Step 10/10: Running simulation for " << config->simDuration << " seconds...\n";
    
    auto simStartTime = std::chrono::high_resolution_clock::now();
    
    Simulator::Stop(Seconds(config->simDuration));
    Simulator::Run();
    
    posFile.close();
    
    auto simEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds simDuration = std::chrono::duration_cast<std::chrono::milliseconds>(simEndTime - simStartTime);
    
    std::cout << "\n✓ Simulation complete";
    std::cout << "  [Simulation runtime: " << std::fixed << std::setprecision(2) 
              << (simDuration.count() / 1000.0) << "s]\n";

    // =================================================================
    // Analyze Results
    // =================================================================
    AnalyzeFlowMonitor(monitor, classifier, ueNodes.GetN());

    // =================================================================
    // Final Summary
    // =================================================================
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║              TEST COMPLETE                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Output files:\n";
    std::cout << "  - output/ue_positions.csv\n\n";

    Simulator::Destroy();
    
    // =================================================================
    // STOP TIMER AND CALCULATE EXECUTION TIME
    // =================================================================
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double totalSeconds = duration.count() / 1000.0;
    int hours = static_cast<int>(totalSeconds / 3600);
    int minutes = static_cast<int>((totalSeconds - hours * 3600) / 60);
    double seconds = totalSeconds - hours * 3600 - minutes * 60;
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║              EXECUTION TIME SUMMARY                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "⏱️  Total wall-clock time: ";
    if (hours > 0)
    {
        std::cout << hours << "h " << minutes << "m " 
                  << std::fixed << std::setprecision(2) << seconds << "s";
    }
    else if (minutes > 0)
    {
        std::cout << minutes << "m " 
                  << std::fixed << std::setprecision(2) << seconds << "s";
    }
    else
    {
        std::cout << std::fixed << std::setprecision(2) << seconds << " seconds";
    }
    std::cout << "\n";
    
    std::cout << "   Raw: " << duration.count() << " milliseconds\n\n";
    
    // Performance analysis
    std::cout << "Performance analysis:\n";
    std::cout << "  Simulated time:     " << config->simDuration << " seconds\n";
    std::cout << "  Real execution time: " << std::fixed << std::setprecision(2) 
              << totalSeconds << " seconds\n";
    std::cout << "  Real-time ratio:    " 
              << std::fixed << std::setprecision(2)
              << (config->simDuration / totalSeconds) << "x\n";
    std::cout << "  (Ratio > 1.0 means simulation ran faster than real-time)\n\n";
    
    // Quick reference
    std::cout << "Time breakdown:\n";
    std::cout << "  Setup (Steps 1-9): See individual step times above\n";
    std::cout << "  Simulation run:    " 
              << std::fixed << std::setprecision(2) 
              << (simDuration.count() / 1000.0) << " seconds\n";
    std::cout << "  Analysis & cleanup: " 
              << std::fixed << std::setprecision(2)
              << (totalSeconds - (simDuration.count() / 1000.0)) << " seconds\n\n";
    
    return 0;
}