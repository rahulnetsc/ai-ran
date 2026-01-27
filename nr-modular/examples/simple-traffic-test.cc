/*
 * Copyright (c) 2026 ARTPARK
 *
 * SIMPLE TRAFFIC TEST - Standalone Example
 * Run this to quickly verify traffic flows at configured rates
 *
 * Location: contrib/nr-modular/examples/simple-traffic-test.cc
 *
 * Usage:
 *   ./ns3 run "simple-traffic-test --dlRate=10 --ulRate=5 --duration=10"
 */

#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

// NR Modular includes
#include "ns3/nr-simulation-manager.h"
#include "ns3/utils/nr-sim-config.h"

#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTrafficTest");

// ============================================================================
// GLOBAL STATISTICS
// ============================================================================

struct FlowStats
{
    uint32_t flowId;
    Ipv4Address src;
    Ipv4Address dst;
    uint16_t srcPort;
    uint16_t dstPort;
    std::string direction;  // "DL" or "UL"
    
    uint64_t txBytes;
    uint64_t rxBytes;
    uint32_t txPackets;
    uint32_t rxPackets;
    uint32_t lostPackets;
    
    double throughputMbps;
    double avgDelayMs;
    double jitterMs;
    double lossPercent;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::vector<FlowStats>
AnalyzeFlows(Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier)
{
    std::vector<FlowStats> results;
    
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    
    for (auto& [flowId, fs] : stats)
    {
        FlowStats flow;
        flow.flowId = flowId;
        
        // Get 5-tuple
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        flow.src = t.sourceAddress;
        flow.dst = t.destinationAddress;
        flow.srcPort = t.sourcePort;
        flow.dstPort = t.destinationPort;
        
        // Determine direction (remote host is 1.0.0.2)
        if (t.sourceAddress.Get() == Ipv4Address("1.0.0.2").Get())
        {
            flow.direction = "DL";
        }
        else if (t.destinationAddress.Get() == Ipv4Address("1.0.0.2").Get())
        {
            flow.direction = "UL";
        }
        else
        {
            flow.direction = "??";
        }
        
        // Basic stats
        flow.txBytes = fs.txBytes;
        flow.rxBytes = fs.rxBytes;
        flow.txPackets = fs.txPackets;
        flow.rxPackets = fs.rxPackets;
        flow.lostPackets = fs.lostPackets;
        
        // Calculate metrics
        double timeSeconds = fs.timeLastRxPacket.GetSeconds() - fs.timeFirstTxPacket.GetSeconds();
        
        if (timeSeconds > 0 && fs.rxBytes > 0)
        {
            flow.throughputMbps = (fs.rxBytes * 8.0) / timeSeconds / 1e6;
        }
        else
        {
            flow.throughputMbps = 0.0;
        }
        
        if (fs.rxPackets > 0)
        {
            flow.avgDelayMs = (fs.delaySum.GetSeconds() / fs.rxPackets) * 1000.0;
            flow.jitterMs = (fs.jitterSum.GetSeconds() / (fs.rxPackets - 1)) * 1000.0;
        }
        else
        {
            flow.avgDelayMs = 0.0;
            flow.jitterMs = 0.0;
        }
        
        if (fs.txPackets > 0)
        {
            flow.lossPercent = (fs.lostPackets * 100.0) / fs.txPackets;
        }
        else
        {
            flow.lossPercent = 0.0;
        }
        
        results.push_back(flow);
    }
    
    return results;
}

void
PrintFlowTable(const std::vector<FlowStats>& flows)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                         FLOW STATISTICS SUMMARY                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;
    
    // Header
    std::cout << std::left
              << std::setw(8) << "Flow"
              << std::setw(4) << "Dir"
              << std::setw(18) << "Source"
              << std::setw(18) << "Destination"
              << std::right
              << std::setw(10) << "Thput"
              << std::setw(8) << "Delay"
              << std::setw(8) << "Jitter"
              << std::setw(7) << "Loss"
              << std::endl;
    
    std::cout << std::left
              << std::setw(8) << "ID"
              << std::setw(4) << ""
              << std::setw(18) << "(IP:Port)"
              << std::setw(18) << "(IP:Port)"
              << std::right
              << std::setw(10) << "(Mbps)"
              << std::setw(8) << "(ms)"
              << std::setw(8) << "(ms)"
              << std::setw(7) << "(%)"
              << std::endl;
    
    std::cout << std::string(80, '─') << std::endl;
    
    // Data rows
    for (const auto& flow : flows)
    {
        std::ostringstream srcStr, dstStr;
        srcStr << flow.src << ":" << flow.srcPort;
        dstStr << flow.dst << ":" << flow.dstPort;
        
        std::cout << std::left
                  << std::setw(8) << flow.flowId
                  << std::setw(4) << flow.direction
                  << std::setw(18) << srcStr.str()
                  << std::setw(18) << dstStr.str()
                  << std::right
                  << std::setw(10) << std::fixed << std::setprecision(2) << flow.throughputMbps
                  << std::setw(8) << std::fixed << std::setprecision(2) << flow.avgDelayMs
                  << std::setw(8) << std::fixed << std::setprecision(2) << flow.jitterMs
                  << std::setw(6) << std::fixed << std::setprecision(1) << flow.lossPercent << "%"
                  << std::endl;
    }
    
    std::cout << std::string(80, '─') << std::endl;
}

void
PrintSummary(const std::vector<FlowStats>& flows, double expectedDl, double expectedUl)
{
    uint32_t dlCount = 0, ulCount = 0;
    double dlTotalThroughput = 0.0, ulTotalThroughput = 0.0;
    double dlAvgDelay = 0.0, ulAvgDelay = 0.0;
    uint32_t dlPass = 0, ulPass = 0;
    
    for (const auto& flow : flows)
    {
        if (flow.direction == "DL")
        {
            dlCount++;
            dlTotalThroughput += flow.throughputMbps;
            dlAvgDelay += flow.avgDelayMs;
            
            // Check if within ±25% of expected
            double tolerance = 0.25;
            if (flow.throughputMbps >= expectedDl * (1.0 - tolerance) &&
                flow.throughputMbps <= expectedDl * (1.0 + tolerance))
            {
                dlPass++;
            }
        }
        else if (flow.direction == "UL")
        {
            ulCount++;
            ulTotalThroughput += flow.throughputMbps;
            ulAvgDelay += flow.avgDelayMs;
            
            double tolerance = 0.25;
            if (flow.throughputMbps >= expectedUl * (1.0 - tolerance) &&
                flow.throughputMbps <= expectedUl * (1.0 + tolerance))
            {
                ulPass++;
            }
        }
    }
    
    if (dlCount > 0) dlAvgDelay /= dlCount;
    if (ulCount > 0) ulAvgDelay /= ulCount;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                              TEST SUMMARY                                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\n┌─ DOWNLINK ─────────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Flows:          " << dlCount << std::endl;
    std::cout << "│ Expected Rate:  " << expectedDl << " Mbps per flow" << std::endl;
    std::cout << "│ Total Thput:    " << std::fixed << std::setprecision(2) << dlTotalThroughput << " Mbps" << std::endl;
    std::cout << "│ Avg Thput:      " << std::fixed << std::setprecision(2) 
              << (dlCount > 0 ? dlTotalThroughput / dlCount : 0.0) << " Mbps per flow" << std::endl;
    std::cout << "│ Avg Delay:      " << std::fixed << std::setprecision(2) << dlAvgDelay << " ms" << std::endl;
    std::cout << "│ Passed:         " << dlPass << "/" << dlCount 
              << (dlPass == dlCount ? " ✓" : " ✗") << std::endl;
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ UPLINK ───────────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Flows:          " << ulCount << std::endl;
    std::cout << "│ Expected Rate:  " << expectedUl << " Mbps per flow" << std::endl;
    std::cout << "│ Total Thput:    " << std::fixed << std::setprecision(2) << ulTotalThroughput << " Mbps" << std::endl;
    std::cout << "│ Avg Thput:      " << std::fixed << std::setprecision(2) 
              << (ulCount > 0 ? ulTotalThroughput / ulCount : 0.0) << " Mbps per flow" << std::endl;
    std::cout << "│ Avg Delay:      " << std::fixed << std::setprecision(2) << ulAvgDelay << " ms" << std::endl;
    std::cout << "│ Passed:         " << ulPass << "/" << ulCount 
              << (ulPass == ulCount ? " ✓" : " ✗") << std::endl;
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ OVERALL ──────────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Total Flows:    " << (dlCount + ulCount) << std::endl;
    std::cout << "│ Total Thput:    " << std::fixed << std::setprecision(2) 
              << (dlTotalThroughput + ulTotalThroughput) << " Mbps" << std::endl;
    std::cout << "│ Passed:         " << (dlPass + ulPass) << "/" << (dlCount + ulCount);
    
    if ((dlPass == dlCount) && (ulPass == ulCount))
    {
        std::cout << " ✓ ALL TESTS PASSED!" << std::endl;
    }
    else
    {
        std::cout << " ✗ SOME TESTS FAILED!" << std::endl;
    }
    
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘" << std::endl;
}

// ============================================================================
// MAIN
// ============================================================================

int
main(int argc, char* argv[])
{
    // Default parameters
    double dlRate = 10.0;   // Mbps per UE
    double ulRate = 5.0;    // Mbps per UE
    double duration = 15.0; // seconds
    uint32_t numUes = 3;
    bool verbose = false;
    
    // Command line
    CommandLine cmd;
    cmd.AddValue("dlRate", "Downlink rate per UE (Mbps)", dlRate);
    cmd.AddValue("ulRate", "Uplink rate per UE (Mbps)", ulRate);
    cmd.AddValue("duration", "Simulation duration (seconds)", duration);
    cmd.AddValue("numUes", "Number of UEs", numUes);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);
    
    // Enable logging if requested
    if (verbose)
    {
        LogComponentEnable("SimpleTrafficTest", LOG_LEVEL_INFO);
        LogComponentEnable("NrSimulationManager", LOG_LEVEL_INFO);
        LogComponentEnable("NrTrafficManager", LOG_LEVEL_INFO);
    }
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                   NR MODULAR - TRAFFIC FLOW TEST                           ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nTest Configuration:" << std::endl;
    std::cout << "  DL Rate:    " << dlRate << " Mbps per UE" << std::endl;
    std::cout << "  UL Rate:    " << ulRate << " Mbps per UE" << std::endl;
    std::cout << "  Duration:   " << duration << " seconds" << std::endl;
    std::cout << "  UEs:        " << numUes << std::endl;
    std::cout << "  Scenario:   1 gNB, UMa, 4 GHz, 20 MHz" << std::endl;
    
    // Create configuration
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
    
    config->topology.gnbCount = 1;
    config->topology.ueCount = numUes;
    config->topology.areaSize = 1000.0;
    config->topology.useFilePositions = false;
    config->topology.uePlacementStrategy = "circle";
    
    config->channel.propagationModel = "UMa";
    config->channel.frequency = 4.0e9;
    config->channel.bandwidth = 20e6;
    
    config->mobility.defaultModel = "ConstantPosition";
    config->mobility.defaultSpeed = 0.0;
    
    config->traffic.udpRateDl = dlRate;
    config->traffic.packetSizeDl = 1024;
    config->traffic.udpRateUl = ulRate;
    config->traffic.packetSizeUl = 512;
    
    config->simDuration = duration;
    config->enableFlowMonitor = true;
    
    // Create and run simulation
    Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
    sim->SetConfig(config);
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 1: INITIALIZATION" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    sim->Initialize();
    
    // Setup FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 2: RUNNING SIMULATION" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    sim->Run();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 3: COLLECTING RESULTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    // Analyze flows
    Ptr<Ipv4FlowClassifier> classifier = 
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    
    std::vector<FlowStats> flows = AnalyzeFlows(flowMonitor, classifier);
    
    // Print results
    PrintFlowTable(flows);
    PrintSummary(flows, dlRate, ulRate);
    
    // Finalize
    sim->Finalize();
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                            TEST COMPLETE                                   ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    return 0;
}
