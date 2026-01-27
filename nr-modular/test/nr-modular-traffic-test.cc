/*
 * Copyright (c) 2026 ARTPARK
 *
 * COMPREHENSIVE TRAFFIC FLOW TEST
 * Tests DL, UL, and simultaneous bidirectional traffic
 * Verifies data rates match configuration
 *
 * Location: contrib/nr-modular/test/nr-modular-traffic-test.cc
 */

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

// NR Modular includes
#include "ns3/nr-simulation-manager.h"
#include "ns3/nr-config-manager.h"
#include "ns3/nr-topology-manager.h"
#include "ns3/nr-network-manager.h"
#include "ns3/nr-mobility-manager.h"
#include "ns3/nr-traffic-manager.h"
// #include "ns3/utils/nr-sim-config.h"
#include "ns3/nr-sim-config.h"

#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrModularTrafficTest");

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Create a simple test configuration
 * 1 gNB, 3 UEs, short simulation
 */
Ptr<NrSimConfig>
CreateTestConfig(double dlRateMbps, double ulRateMbps, double simDuration)
{
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
    
    // Topology: Simple single-cell
    config->topology.gnbCount = 1;
    config->topology.ueCount = 3;
    config->topology.areaSize = 1000.0;
    config->topology.useFilePositions = false;
    config->topology.uePlacementStrategy = "circle";  // UEs in circle around gNB
    
    // Channel: UMa scenario, 4 GHz
    config->channel.propagationModel = "UMa";
    config->channel.frequency = 4.0e9;  // 4 GHz
    config->channel.bandwidth = 20e6;   // 20 MHz
    
    // Mobility: Static UEs for predictable results
    config->mobility.defaultModel = "ConstantPosition";
    config->mobility.defaultSpeed = 0.0;
    
    // Traffic: Configurable rates
    config->traffic.udpRateDl = dlRateMbps;
    config->traffic.packetSizeDl = 1024;
    config->traffic.udpRateUl = ulRateMbps;
    config->traffic.packetSizeUl = 512;
    
    // Simulation
    config->simDuration = simDuration;
    
    // Metrics
    config->enableFlowMonitor = true;
    config->outputFilePath = "test-results.csv";
    
    return config;
}

/**
 * Measure throughput using FlowMonitor
 * Returns map: flowId -> throughput (Mbps)
 */
std::map<uint32_t, double>
MeasureThroughput(Ptr<FlowMonitor> flowMonitor)
{
    std::map<uint32_t, double> throughputs;
    
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    
    for (auto& flow : stats)
    {
        uint32_t flowId = flow.first;
        FlowMonitor::FlowStats& fs = flow.second;
        
        // Calculate throughput: (bytes received * 8) / time / 1e6
        double timeSeconds = fs.timeLastRxPacket.GetSeconds() - fs.timeFirstTxPacket.GetSeconds();
        
        if (timeSeconds > 0 && fs.rxBytes > 0)
        {
            double throughputMbps = (fs.rxBytes * 8.0) / timeSeconds / 1e6;
            throughputs[flowId] = throughputMbps;
            
            NS_LOG_INFO("Flow " << flowId << ": " 
                        << throughputMbps << " Mbps "
                        << "(" << fs.txPackets << " tx, " 
                        << fs.rxPackets << " rx, "
                        << fs.lostPackets << " lost)");
        }
    }
    
    return throughputs;
}

/**
 * Check if measured throughput is within tolerance of expected
 * Tolerance: ±20% (accounts for MAC overhead, scheduling)
 */
bool
VerifyThroughput(double measured, double expected, double tolerancePercent = 20.0)
{
    double lowerBound = expected * (1.0 - tolerancePercent / 100.0);
    double upperBound = expected * (1.0 + tolerancePercent / 100.0);
    
    bool ok = (measured >= lowerBound && measured <= upperBound);
    
    NS_LOG_INFO("Throughput check: measured=" << measured << " Mbps, "
                << "expected=" << expected << " Mbps, "
                << "range=[" << lowerBound << ", " << upperBound << "] Mbps, "
                << (ok ? "PASS" : "FAIL"));
    
    return ok;
}

// ============================================================================
// TEST CASE 1: DOWNLINK ONLY
// ============================================================================

class NrModularDlOnlyTest : public TestCase
{
public:
    NrModularDlOnlyTest()
        : TestCase("NR Modular - Downlink Only Traffic Test")
    {
    }
    
    virtual ~NrModularDlOnlyTest()
    {
    }

private:
    virtual void DoRun()
    {
        NS_LOG_FUNCTION(this);
        
        std::cout << "\n============================================" << std::endl;
        std::cout << "TEST: Downlink Only Traffic" << std::endl;
        std::cout << "============================================" << std::endl;
        
        // Test parameters
        double dlRate = 10.0;  // 10 Mbps per UE
        double ulRate = 0.0;   // No uplink
        double simTime = 10.0; // 10 seconds
        
        std::cout << "Config: DL=" << dlRate << " Mbps, UL=" << ulRate << " Mbps, "
                  << "Duration=" << simTime << " s" << std::endl;
        
        // Create configuration
        Ptr<NrSimConfig> config = CreateTestConfig(dlRate, ulRate, simTime);
        
        // Create simulation manager
        Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
        sim->SetConfig(config);
        
        // Initialize
        std::cout << "\nInitializing simulation..." << std::endl;
        sim->Initialize();
        
        // Setup FlowMonitor
        FlowMonitorHelper flowHelper;
        Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
        
        // Run simulation
        std::cout << "\nRunning simulation..." << std::endl;
        sim->Run();
        
        // Measure throughput
        std::cout << "\nMeasuring throughput..." << std::endl;
        std::map<uint32_t, double> throughputs = MeasureThroughput(flowMonitor);
        
        // Verify results
        std::cout << "\nVerifying results..." << std::endl;
        
        uint32_t dlFlows = 0;
        uint32_t dlPass = 0;
        
        Ptr<Ipv4FlowClassifier> classifier = 
            DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        
        for (auto& [flowId, throughput] : throughputs)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
            
            // Check if this is a downlink flow (src=1.0.0.2, dst=7.0.0.x)
            if (t.sourceAddress.Get() == Ipv4Address("1.0.0.2").Get())
            {
                dlFlows++;
                std::cout << "  DL Flow " << flowId << ": " 
                          << t.sourceAddress << " -> " << t.destinationAddress 
                          << " = " << throughput << " Mbps" << std::endl;
                
                if (VerifyThroughput(throughput, dlRate))
                {
                    dlPass++;
                }
            }
        }
        
        std::cout << "\nResults: " << dlPass << "/" << dlFlows << " DL flows passed" << std::endl;
        
        // Cleanup
        sim->Finalize();
        
        // Test assertions
        NS_TEST_ASSERT_MSG_GT(dlFlows, 0, "No downlink flows detected!");
        NS_TEST_ASSERT_MSG_EQ(dlPass, dlFlows, "Not all DL flows achieved target rate!");
        
        std::cout << "✓ TEST PASSED" << std::endl;
        std::cout << "============================================\n" << std::endl;
    }
};

// ============================================================================
// TEST CASE 2: UPLINK ONLY
// ============================================================================

class NrModularUlOnlyTest : public TestCase
{
public:
    NrModularUlOnlyTest()
        : TestCase("NR Modular - Uplink Only Traffic Test")
    {
    }
    
    virtual ~NrModularUlOnlyTest()
    {
    }

private:
    virtual void DoRun()
    {
        NS_LOG_FUNCTION(this);
        
        std::cout << "\n============================================" << std::endl;
        std::cout << "TEST: Uplink Only Traffic" << std::endl;
        std::cout << "============================================" << std::endl;
        
        // Test parameters
        double dlRate = 0.0;   // No downlink
        double ulRate = 5.0;   // 5 Mbps per UE
        double simTime = 10.0;
        
        std::cout << "Config: DL=" << dlRate << " Mbps, UL=" << ulRate << " Mbps, "
                  << "Duration=" << simTime << " s" << std::endl;
        
        // Create configuration
        Ptr<NrSimConfig> config = CreateTestConfig(dlRate, ulRate, simTime);
        
        // Create simulation manager
        Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
        sim->SetConfig(config);
        
        // Initialize
        std::cout << "\nInitializing simulation..." << std::endl;
        sim->Initialize();
        
        // Setup FlowMonitor
        FlowMonitorHelper flowHelper;
        Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
        
        // Run simulation
        std::cout << "\nRunning simulation..." << std::endl;
        sim->Run();
        
        // Measure throughput
        std::cout << "\nMeasuring throughput..." << std::endl;
        std::map<uint32_t, double> throughputs = MeasureThroughput(flowMonitor);
        
        // Verify results
        std::cout << "\nVerifying results..." << std::endl;
        
        uint32_t ulFlows = 0;
        uint32_t ulPass = 0;
        
        Ptr<Ipv4FlowClassifier> classifier = 
            DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        
        for (auto& [flowId, throughput] : throughputs)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
            
            // Check if this is an uplink flow (src=7.0.0.x, dst=1.0.0.2)
            if (t.sourceAddress.Get() != Ipv4Address("1.0.0.2").Get() &&
                t.destinationAddress.Get() == Ipv4Address("1.0.0.2").Get())
            {
                ulFlows++;
                std::cout << "  UL Flow " << flowId << ": " 
                          << t.sourceAddress << " -> " << t.destinationAddress 
                          << " = " << throughput << " Mbps" << std::endl;
                
                if (VerifyThroughput(throughput, ulRate))
                {
                    ulPass++;
                }
            }
        }
        
        std::cout << "\nResults: " << ulPass << "/" << ulFlows << " UL flows passed" << std::endl;
        
        // Cleanup
        sim->Finalize();
        
        // Test assertions
        NS_TEST_ASSERT_MSG_GT(ulFlows, 0, "No uplink flows detected!");
        NS_TEST_ASSERT_MSG_EQ(ulPass, ulFlows, "Not all UL flows achieved target rate!");
        
        std::cout << "✓ TEST PASSED" << std::endl;
        std::cout << "============================================\n" << std::endl;
    }
};

// ============================================================================
// TEST CASE 3: SIMULTANEOUS BIDIRECTIONAL
// ============================================================================

class NrModularBidirectionalTest : public TestCase
{
public:
    NrModularBidirectionalTest()
        : TestCase("NR Modular - Simultaneous Bidirectional Traffic Test")
    {
    }
    
    virtual ~NrModularBidirectionalTest()
    {
    }

private:
    virtual void DoRun()
    {
        NS_LOG_FUNCTION(this);
        
        std::cout << "\n============================================" << std::endl;
        std::cout << "TEST: Simultaneous Bidirectional Traffic" << std::endl;
        std::cout << "============================================" << std::endl;
        
        // Test parameters - Realistic bidirectional load
        double dlRate = 10.0;  // 10 Mbps per UE
        double ulRate = 5.0;   // 5 Mbps per UE
        double simTime = 15.0; // Longer for stability
        
        std::cout << "Config: DL=" << dlRate << " Mbps, UL=" << ulRate << " Mbps, "
                  << "Duration=" << simTime << " s" << std::endl;
        
        // Create configuration
        Ptr<NrSimConfig> config = CreateTestConfig(dlRate, ulRate, simTime);
        
        // Create simulation manager
        Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
        sim->SetConfig(config);
        
        // Initialize
        std::cout << "\nInitializing simulation..." << std::endl;
        sim->Initialize();
        
        // Setup FlowMonitor
        FlowMonitorHelper flowHelper;
        Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
        
        // Run simulation
        std::cout << "\nRunning simulation..." << std::endl;
        sim->Run();
        
        // Measure throughput
        std::cout << "\nMeasuring throughput..." << std::endl;
        std::map<uint32_t, double> throughputs = MeasureThroughput(flowMonitor);
        
        // Verify results
        std::cout << "\nVerifying results..." << std::endl;
        
        uint32_t dlFlows = 0, dlPass = 0;
        uint32_t ulFlows = 0, ulPass = 0;
        
        Ptr<Ipv4FlowClassifier> classifier = 
            DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        
        for (auto& [flowId, throughput] : throughputs)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
            
            // Downlink: src=1.0.0.2
            if (t.sourceAddress.Get() == Ipv4Address("1.0.0.2").Get())
            {
                dlFlows++;
                std::cout << "  DL Flow " << flowId << ": " 
                          << t.sourceAddress << " -> " << t.destinationAddress 
                          << " = " << throughput << " Mbps" << std::endl;
                
                if (VerifyThroughput(throughput, dlRate, 25.0))  // Wider tolerance for bidirectional
                {
                    dlPass++;
                }
            }
            // Uplink: dst=1.0.0.2
            else if (t.destinationAddress.Get() == Ipv4Address("1.0.0.2").Get())
            {
                ulFlows++;
                std::cout << "  UL Flow " << flowId << ": " 
                          << t.sourceAddress << " -> " << t.destinationAddress 
                          << " = " << throughput << " Mbps" << std::endl;
                
                if (VerifyThroughput(throughput, ulRate, 25.0))  // Wider tolerance for bidirectional
                {
                    ulPass++;
                }
            }
        }
        
        std::cout << "\nResults:" << std::endl;
        std::cout << "  DL: " << dlPass << "/" << dlFlows << " flows passed" << std::endl;
        std::cout << "  UL: " << ulPass << "/" << ulFlows << " flows passed" << std::endl;
        
        // Cleanup
        sim->Finalize();
        
        // Test assertions
        NS_TEST_ASSERT_MSG_GT(dlFlows, 0, "No downlink flows detected!");
        NS_TEST_ASSERT_MSG_GT(ulFlows, 0, "No uplink flows detected!");
        NS_TEST_ASSERT_MSG_GT(dlPass, dlFlows * 0.66, "Too many DL flows below target!");
        NS_TEST_ASSERT_MSG_GT(ulPass, ulFlows * 0.66, "Too many UL flows below target!");
        
        std::cout << "✓ TEST PASSED" << std::endl;
        std::cout << "============================================\n" << std::endl;
    }
};

// ============================================================================
// TEST SUITE
// ============================================================================

class NrModularTrafficTestSuite : public TestSuite
{
public:
    NrModularTrafficTestSuite()
        : TestSuite("nr-modular-traffic", TestSuite::UNIT) // Fix: Added TestSuite:: scope
    {
        // Add test cases
        // Fix: Added TestCase:: scope for QUICK
        AddTestCase(new NrModularDlOnlyTest(), TestCase::QUICK);
        AddTestCase(new NrModularUlOnlyTest(), TestCase::QUICK);
        AddTestCase(new NrModularBidirectionalTest(), TestCase::QUICK);
    }
};

// Static variable to register the test suite with the global runner
static NrModularTrafficTestSuite g_nrModularTrafficTestSuite;

// ============================================================================
// STANDALONE MAIN FOR SCRATCH
// ============================================================================

int main (int argc, char *argv[])
{
  // This allows the scratch program to run the test suite manually
  // without needing the ./test.py script.
  NrModularTrafficTestSuite testSuite;
  
  // To run all tests in the suite from a standalone main:
  // We use the TestRunner to filter and run the specific suite name.
  bool result = TestRunner::Run(argc, argv);
  
  return result ? 0 : 1;
}