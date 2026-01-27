/*
 * Copyright (c) 2026 ARTPARK
 *
 * ENHANCED VERSION - Added comprehensive RX diagnostics
 */

#include "nr-traffic-manager.h"
#include "utils/nr-sim-config.h"
#include "nr-network-manager.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-helper.h"

#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4.h"
#include "ns3/packet-sink.h"

#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrTrafficManager");
NS_OBJECT_ENSURE_REGISTERED(NrTrafficManager);

TypeId
NrTrafficManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrTrafficManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrTrafficManager>();
    return tid;
}

#include <map>

// Global map to track print counts for each debug context
static std::map<std::string, uint32_t> g_traceCounters;
const uint32_t MAX_DEBUG_PRINTS = 15; // Increased for better visibility

// Enhanced PacketSink Rx tracer with more details
static void
AppRxTracer(std::string context, Ptr<const Packet> packet, const Address &address)
{
    if (g_traceCounters[context] < MAX_DEBUG_PRINTS)
    {
        std::cout << "  [✓ APP RX] " << context << " received " << packet->GetSize() 
                  << " bytes from " << InetSocketAddress::ConvertFrom(address).GetIpv4() 
                  << ":" << InetSocketAddress::ConvertFrom(address).GetPort()
                  << " (#" << ++g_traceCounters[context] << ")" << std::endl;
        
        if (g_traceCounters[context] == MAX_DEBUG_PRINTS)
            std::cout << "  [APP RX] " << context << " - Max prints reached. Silencing..." << std::endl;
    }
}

// Enhanced Ipv4 Tx/Rx tracer
static void
Ipv4Tracer(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    if (g_traceCounters[context] < MAX_DEBUG_PRINTS)
    {
        // Get source and destination from packet
        Ptr<Packet> copy = packet->Copy();
        Ipv4Header ipv4Header;
        copy->PeekHeader(ipv4Header);
        
        std::cout << "  [L3] " << context 
                  << " | IF:" << interface 
                  << " | " << ipv4Header.GetSource() << " → " << ipv4Header.GetDestination()
                  << " | " << packet->GetSize() << " bytes"
                  << " (#" << ++g_traceCounters[context] << ")" << std::endl;

        if (g_traceCounters[context] == MAX_DEBUG_PRINTS)
            std::cout << "  [L3] " << context << " - Max prints reached. Silencing..." << std::endl;
    }
}

// Enhanced drop tracer with reason details
static void
Ipv4DropTracer(std::string context, const Ipv4Header &header, Ptr<const Packet> packet, 
               Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t interface)
{
    std::string reasonStr;
    switch(reason)
    {
        case Ipv4L3Protocol::DROP_TTL_EXPIRED: reasonStr = "TTL_EXPIRED"; break;
        case Ipv4L3Protocol::DROP_NO_ROUTE: reasonStr = "NO_ROUTE"; break;
        case Ipv4L3Protocol::DROP_BAD_CHECKSUM: reasonStr = "BAD_CHECKSUM"; break;
        case Ipv4L3Protocol::DROP_INTERFACE_DOWN: reasonStr = "INTERFACE_DOWN"; break;
        case Ipv4L3Protocol::DROP_ROUTE_ERROR: reasonStr = "ROUTE_ERROR"; break;
        default: reasonStr = "UNKNOWN(" + std::to_string(reason) + ")"; break;
    }
    
    // Always print drops (no limit)
    std::cout << "  [!!! DROP !!!] " << context 
              << " | Reason: " << reasonStr
              << " | " << header.GetSource() << " → " << header.GetDestination()
              << " | IF:" << interface
              << " | " << packet->GetSize() << " bytes" 
              << std::endl;
}

NrTrafficManager::NrTrafficManager()
    : m_config(nullptr),
      m_networkManager(nullptr),
      m_installed(false)
{
}

NrTrafficManager::~NrTrafficManager()
{
}

void
NrTrafficManager::DoDispose()
{
    m_config = nullptr;
    m_networkManager = nullptr;
    
    m_dlServerApps = ApplicationContainer();
    m_dlClientApps = ApplicationContainer();
    m_ulServerApps = ApplicationContainer();
    m_ulClientApps = ApplicationContainer();
    m_serverApps = ApplicationContainer();
    m_clientApps = ApplicationContainer();
    
    m_installed = false;
    Object::DoDispose();
}

void
NrTrafficManager::SetNetworkManager(Ptr<NrNetworkManager> netMgr)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(netMgr == nullptr, "NetworkManager cannot be null");
    m_networkManager = netMgr;
}

void
NrTrafficManager::SetConfig(const Ptr<NrSimConfig>& config)
{
    NS_ABORT_MSG_IF(config == nullptr, "NrTrafficManager: config cannot be null");
    m_config = config;
}

void
NrTrafficManager::InstallTraffic(const NodeContainer& gnbNodes,
                                 const NodeContainer& ueNodes)
{
    NS_ABORT_MSG_IF(m_config == nullptr, "Config must be set before installing traffic");
    NS_ABORT_MSG_IF(m_networkManager == nullptr, "NetworkManager must be set before installing traffic");  
    NS_ABORT_MSG_IF(m_installed, "Traffic already installed");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Installing UDP traffic applications" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "gNBs: " << gnbNodes.GetN() << std::endl;
    std::cout << "UEs: " << ueNodes.GetN() << std::endl;

    std::string dlRate = std::to_string(m_config->traffic.udpRateDl) + "Mbps";
    std::string ulRate = std::to_string(m_config->traffic.udpRateUl) + "Mbps";
    uint32_t dlPacketSize = m_config->traffic.packetSizeDl;
    uint32_t ulPacketSize = m_config->traffic.packetSizeUl;
    
    std::cout << "Traffic config:" << std::endl;
    std::cout << "  DL: " << dlRate << " (" << dlPacketSize << " bytes)" << std::endl;
    std::cout << "  UL: " << ulRate << " (" << ulPacketSize << " bytes)" << std::endl;

    // Get UE IP addresses
    Ipv4InterfaceContainer ueIpIfaces = m_networkManager->GetUeIpInterfaces();
    
    std::cout << "\nUE IP addresses:" << std::endl;
    for (uint32_t i = 0; i < ueIpIfaces.GetN(); ++i)
    {
        std::cout << "  UE " << i << ": " << ueIpIfaces.GetAddress(i, 0) << std::endl;
    }

    // Create Remote Host
    std::cout << "\nCreating remote host..." << std::endl;
    
    Ptr<Node> pgw = m_networkManager->GetEpcHelper()->GetPgwNode();
    Ptr<Node> remoteHost = CreateObject<Node>();
    
    InternetStackHelper internet;
    internet.Install(remoteHost);
    
    std::cout << "  ✓ Remote host created (Node ID: " << remoteHost->GetId() << ")" << std::endl;

    // Create P2P link
    std::cout << "Connecting remote host to PGW..." << std::endl;
    
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    
    NodeContainer internetNodes;
    internetNodes.Add(pgw);
    internetNodes.Add(remoteHost);
    NetDeviceContainer internetDevices = p2ph.Install(internetNodes);
    
    std::cout << "  ✓ P2P link created" << std::endl;

    // Assign IP addresses to P2P link
    std::cout << "Assigning IP addresses to P2P link..." << std::endl;
    
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
    
    std::cout << "  PGW (SGi): " << internetIpIfaces.GetAddress(0) << std::endl;
    std::cout << "  Remote host: " << remoteHostAddr << std::endl;

    // Configure routing
    std::cout << "Configuring routing..." << std::endl;
    
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"),
        Ipv4Mask("255.0.0.0"),
        internetIpIfaces.GetAddress(0),  // Next hop = PGW
        1
    );
    
    std::cout << "  ✓ Route added: 7.0.0.0/8 via PGW" << std::endl;

    // Install traffic applications
    std::cout << "\nInstalling traffic applications..." << std::endl;
    
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;
    double startTime = 3.0;  // Increased delay to ensure attachment
    double stopTime = m_config->simDuration;

    // DOWNLINK: Remote → UEs
    std::cout << "  Phase 1: Installing downlink flows..." << std::endl;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ipv4Address ueAddr = ueIpIfaces.GetAddress(i, 0);

        // DL Sink on UE
        PacketSinkHelper dlSink("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), dlPort + i));
        m_dlServerApps.Add(dlSink.Install(ueNodes.Get(i)));
        
        // DL Source on Remote Host
        OnOffHelper dlClient("ns3::UdpSocketFactory",
                            InetSocketAddress(ueAddr, dlPort + i));
        dlClient.SetAttribute("PacketSize", UintegerValue(dlPacketSize));
        dlClient.SetConstantRate(DataRate(dlRate));
        m_dlClientApps.Add(dlClient.Install(remoteHost));
        
        std::cout << "    UE " << i << ": Remote:" << remoteHostAddr << " → UE:" 
                  << ueAddr << ":" << (dlPort + i) << std::endl;
    }
    std::cout << "    ✓ " << ueNodes.GetN() << " DL flows installed" << std::endl;

    // UPLINK: UEs → Remote
    std::cout << "  Phase 2: Installing uplink flows..." << std::endl;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ipv4Address ueAddr = ueIpIfaces.GetAddress(i, 0);
        
        // UL Sink on Remote Host
        PacketSinkHelper ulSink("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), ulPort + i));
        m_ulServerApps.Add(ulSink.Install(remoteHost));
        
        // UL Source on UE
        OnOffHelper ulClient("ns3::UdpSocketFactory",
                            InetSocketAddress(remoteHostAddr, ulPort + i));
        ulClient.SetAttribute("PacketSize", UintegerValue(ulPacketSize));
        ulClient.SetConstantRate(DataRate(ulRate));
        m_ulClientApps.Add(ulClient.Install(ueNodes.Get(i)));
        
        std::cout << "    UE " << i << ": UE:" << ueAddr << " → Remote:" 
                  << remoteHostAddr << ":" << (ulPort + i) << std::endl;
    }
    std::cout << "    ✓ " << ueNodes.GetN() << " UL flows installed" << std::endl;

    // Build combined containers
    for (uint32_t i = 0; i < m_dlServerApps.GetN(); ++i)
        m_serverApps.Add(m_dlServerApps.Get(i));
    for (uint32_t i = 0; i < m_ulServerApps.GetN(); ++i)
        m_serverApps.Add(m_ulServerApps.Get(i));
    for (uint32_t i = 0; i < m_dlClientApps.GetN(); ++i)
        m_clientApps.Add(m_dlClientApps.Get(i));
    for (uint32_t i = 0; i < m_ulClientApps.GetN(); ++i)
        m_clientApps.Add(m_ulClientApps.Get(i));

    // Setup comprehensive tracing
    std::cout << "\nSetting up comprehensive tracing..." << std::endl;
    
    // Trace DL application layer (sinks on UEs)
    for (uint32_t i = 0; i < m_dlServerApps.GetN(); ++i)
    {
        std::string context = "DL_Sink_UE_" + std::to_string(i);
        m_dlServerApps.Get(i)->TraceConnectWithoutContext(
            "Rx", MakeCallback(&AppRxTracer).Bind(context));
    }
    
    // Trace UL application layer (sinks on remote host)
    for (uint32_t i = 0; i < m_ulServerApps.GetN(); ++i)
    {
        std::string context = "UL_Sink_Remote_" + std::to_string(i);
        m_ulServerApps.Get(i)->TraceConnectWithoutContext(
            "Rx", MakeCallback(&AppRxTracer).Bind(context));
    }

    // Trace Remote Host L3
    Ptr<Ipv4L3Protocol> remoteIpv4 = remoteHost->GetObject<Ipv4L3Protocol>();
    remoteIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4Tracer).Bind("Remote_TX"));
    remoteIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4Tracer).Bind("Remote_RX"));
    remoteIpv4->TraceConnectWithoutContext("Drop", MakeCallback(&Ipv4DropTracer).Bind("Remote_DROP"));

    // Trace UE L3
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4L3Protocol> ueIpv4 = ueNodes.Get(i)->GetObject<Ipv4L3Protocol>();
        std::string txCtx = "UE" + std::to_string(i) + "_TX";
        std::string rxCtx = "UE" + std::to_string(i) + "_RX";
        std::string dropCtx = "UE" + std::to_string(i) + "_DROP";
        
        ueIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4Tracer).Bind(txCtx));
        ueIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4Tracer).Bind(rxCtx));
        ueIpv4->TraceConnectWithoutContext("Drop", MakeCallback(&Ipv4DropTracer).Bind(dropCtx));
    }

    // Trace PGW L3
    Ptr<Ipv4L3Protocol> pgwIpv4 = pgw->GetObject<Ipv4L3Protocol>();
    pgwIpv4->TraceConnectWithoutContext("Tx", MakeCallback(&Ipv4Tracer).Bind("PGW_TX"));
    pgwIpv4->TraceConnectWithoutContext("Rx", MakeCallback(&Ipv4Tracer).Bind("PGW_RX"));
    pgwIpv4->TraceConnectWithoutContext("Drop", MakeCallback(&Ipv4DropTracer).Bind("PGW_DROP"));

    std::cout << "  ✓ Application layer tracing: " << (m_dlServerApps.GetN() + m_ulServerApps.GetN()) << " sinks" << std::endl;
    std::cout << "  ✓ Network layer tracing: Remote, PGW, " << ueNodes.GetN() << " UEs" << std::endl;

    // Schedule applications
    std::cout << "\nScheduling applications..." << std::endl;
    
    // Start sinks first
    m_dlServerApps.Start(Seconds(startTime));
    m_ulServerApps.Start(Seconds(startTime));
    
    // Start sources slightly later
    m_dlClientApps.Start(Seconds(startTime + 0.5));
    m_ulClientApps.Start(Seconds(startTime + 0.5));
    
    // Stop at simulation end
    m_dlServerApps.Stop(Seconds(stopTime));
    m_ulServerApps.Stop(Seconds(stopTime));
    m_dlClientApps.Stop(Seconds(stopTime));
    m_ulClientApps.Stop(Seconds(stopTime));

    std::cout << "  ✓ Applications start at: " << startTime << " s" << std::endl;
    std::cout << "  ✓ Traffic starts at: " << (startTime + 0.5) << " s" << std::endl;
    std::cout << "  ✓ Applications stop at: " << stopTime << " s" << std::endl;

    m_installed = true;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Traffic installation complete!" << std::endl;
    std::cout << "  DL: " << m_dlServerApps.GetN() << " sinks + " << m_dlClientApps.GetN() << " sources" << std::endl;
    std::cout << "  UL: " << m_ulServerApps.GetN() << " sinks + " << m_ulClientApps.GetN() << " sources" << std::endl;
    std::cout << "  Total: " << (m_serverApps.GetN() + m_clientApps.GetN()) << " applications" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::cout << "\n⚠️  IMPORTANT: Watch for RX traces during simulation!" << std::endl;
    std::cout << "Expected traces:" << std::endl;
    std::cout << "  - Remote_RX: Uplink packets arriving at remote host" << std::endl;
    std::cout << "  - UE*_RX: Downlink packets arriving at UEs" << std::endl;
    std::cout << "  - *_DROP: Any dropped packets (investigate if seen!)" << std::endl;
    std::cout << "  - DL_Sink_UE_*: Successful app-layer DL reception" << std::endl;
    std::cout << "  - UL_Sink_Remote_*: Successful app-layer UL reception\n" << std::endl;
}

void
NrTrafficManager::CollectMetrics()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Collecting Traffic Metrics" << std::endl;
    std::cout << "========================================" << std::endl;

    // Instead of FlowMonitor, use PacketSink statistics
    CollectPacketSinkStats();
    ComputeAggregateMetrics();

    m_metricsCollected = true;
    std::cout << "  ✓ Metrics collection complete" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void
NrTrafficManager::CollectPacketSinkStats()
{
    NS_LOG_FUNCTION(this);
    
    std::cout << "Processing PacketSink statistics..." << std::endl;
    
    // Calculate actual traffic duration
    m_trafficDuration = m_config->simDuration - 3.5;  // Apps start at 3.5s
    
    std::cout << "  Traffic duration: " << m_trafficDuration << " seconds" << std::endl;
    
    // Downlink: PacketSinks on UEs
    for (uint32_t i = 0; i < m_dlServerApps.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(m_dlServerApps.Get(i));
        if (sink)
        {
            uint64_t totalRxBytes = sink->GetTotalRx();
            uint64_t rxPackets = totalRxBytes / m_config->traffic.packetSizeDl;
            
            // Calculate expected TX packets
            double expectedBitsPerSec = m_config->traffic.udpRateDl * 1e6;
            uint64_t expectedTxPackets = (expectedBitsPerSec * m_trafficDuration) / 
                                         (m_config->traffic.packetSizeDl * 8);
            
            double throughputMbps = (totalRxBytes * 8.0) / (m_trafficDuration * 1e6);
            
            m_ueMetrics[i].ueId = i;
            m_ueMetrics[i].dlThroughputMbps = throughputMbps;
            m_ueMetrics[i].dlRxBytes = totalRxBytes;
            m_ueMetrics[i].dlRxPackets = rxPackets;
            m_ueMetrics[i].dlTxPackets = expectedTxPackets;
            m_ueMetrics[i].dlLostPackets = (expectedTxPackets > rxPackets) ? 
                                           (expectedTxPackets - rxPackets) : 0;
            
            if (expectedTxPackets > 0)
                m_ueMetrics[i].dlPacketLossRate = 
                    static_cast<double>(m_ueMetrics[i].dlLostPackets) / expectedTxPackets;
        }
    }
    
    // Uplink: PacketSinks on remote host
    for (uint32_t i = 0; i < m_ulServerApps.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(m_ulServerApps.Get(i));
        if (sink)
        {
            uint64_t totalRxBytes = sink->GetTotalRx();
            uint64_t rxPackets = totalRxBytes / m_config->traffic.packetSizeUl;
            
            // Calculate expected TX packets
            double expectedBitsPerSec = m_config->traffic.udpRateUl * 1e6;
            uint64_t expectedTxPackets = (expectedBitsPerSec * m_trafficDuration) / 
                                         (m_config->traffic.packetSizeUl * 8);
            
            double throughputMbps = (totalRxBytes * 8.0) / (m_trafficDuration * 1e6);
            
            m_ueMetrics[i].ulThroughputMbps = throughputMbps;
            m_ueMetrics[i].ulRxBytes = totalRxBytes;
            m_ueMetrics[i].ulRxPackets = rxPackets;
            m_ueMetrics[i].ulTxPackets = expectedTxPackets;
            m_ueMetrics[i].ulLostPackets = (expectedTxPackets > rxPackets) ? 
                                           (expectedTxPackets - rxPackets) : 0;
            
            if (expectedTxPackets > 0)
                m_ueMetrics[i].ulPacketLossRate = 
                    static_cast<double>(m_ueMetrics[i].ulLostPackets) / expectedTxPackets;
        }
    }
    
    std::cout << "  ✓ PacketSink statistics processed" << std::endl;
}

void
NrTrafficManager::ComputeAggregateMetrics()
{
    NS_LOG_FUNCTION(this);
    
    std::cout << "Computing aggregate metrics..." << std::endl;

    m_aggregateMetrics = AggregateMetrics();
    m_aggregateMetrics.numUes = m_ueMetrics.size();
    
    double totalDelay = 0.0;
    uint32_t delayCount = 0;

    for (auto& pair : m_ueMetrics)
    {
        PerUeMetrics& ueMetrics = pair.second;
        
        // Throughput
        m_aggregateMetrics.totalDlThroughputMbps += ueMetrics.dlThroughputMbps;
        m_aggregateMetrics.totalUlThroughputMbps += ueMetrics.ulThroughputMbps;
        
        // Delay
        if (ueMetrics.dlAvgDelayMs > 0)
        {
            totalDelay += ueMetrics.dlAvgDelayMs;
            delayCount++;
        }
        if (ueMetrics.ulAvgDelayMs > 0)
        {
            totalDelay += ueMetrics.ulAvgDelayMs;
            delayCount++;
        }
        
        // Packets
        m_aggregateMetrics.totalPacketsSent += ueMetrics.dlTxPackets + ueMetrics.ulTxPackets;
        m_aggregateMetrics.totalPacketsReceived += ueMetrics.dlRxPackets + ueMetrics.ulRxPackets;
        m_aggregateMetrics.totalPacketsLost += ueMetrics.dlLostPackets + ueMetrics.ulLostPackets;
    }
    
    // Compute averages
    if (m_aggregateMetrics.numUes > 0)
    {
        m_aggregateMetrics.avgDlThroughputMbps = m_aggregateMetrics.totalDlThroughputMbps / m_aggregateMetrics.numUes;
        m_aggregateMetrics.avgUlThroughputMbps = m_aggregateMetrics.totalUlThroughputMbps / m_aggregateMetrics.numUes;
    }
    
    if (delayCount > 0)
    {
        m_aggregateMetrics.avgSystemDelayMs = totalDelay / delayCount;
    }
    
    if (m_aggregateMetrics.totalPacketsSent > 0)
    {
        m_aggregateMetrics.overallPacketLossRate = 
            static_cast<double>(m_aggregateMetrics.totalPacketsLost) / 
            m_aggregateMetrics.totalPacketsSent;
    }
    
    std::cout << "  ✓ Aggregate metrics computed" << std::endl;
}

void
NrTrafficManager::PrintMetricsSummary() const
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_metricsCollected, "Must call CollectMetrics() first");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Traffic Metrics Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    // Aggregate metrics
    std::cout << "\n--- Aggregate Metrics ---" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total DL Throughput: " << m_aggregateMetrics.totalDlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Total UL Throughput: " << m_aggregateMetrics.totalUlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg DL Throughput/UE: " << m_aggregateMetrics.avgDlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg UL Throughput/UE: " << m_aggregateMetrics.avgUlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg System Delay: " << m_aggregateMetrics.avgSystemDelayMs << " ms" << std::endl;
    std::cout << "Overall Packet Loss: " << (m_aggregateMetrics.overallPacketLossRate * 100.0) << "%" << std::endl;

    // Per-UE metrics
    std::cout << "\n--- Per-UE Metrics ---" << std::endl;
    for (const auto& pair : m_ueMetrics)
    {
        const PerUeMetrics& m = pair.second;
        std::cout << "\nUE " << m.ueId << ":" << std::endl;
        
        if (m_config->traffic.enableDownlink)
        {
            std::cout << "  DL: " << m.dlThroughputMbps << " Mbps, " 
                      << m.dlAvgDelayMs << " ms delay, "
                      << (m.dlPacketLossRate * 100.0) << "% loss ("
                      << m.dlRxPackets << "/" << m.dlTxPackets << " pkts)" << std::endl;
        }
        
        if (m_config->traffic.enableUplink)
        {
            std::cout << "  UL: " << m.ulThroughputMbps << " Mbps, "
                      << m.ulAvgDelayMs << " ms delay, "
                      << (m.ulPacketLossRate * 100.0) << "% loss ("
                      << m.ulRxPackets << "/" << m.ulTxPackets << " pkts)" << std::endl;
        }
    }

    std::cout << "========================================\n" << std::endl;
}

ApplicationContainer
NrTrafficManager::GetServerApps() const
{
    return m_serverApps;
}

ApplicationContainer
NrTrafficManager::GetClientApps() const
{
    return m_clientApps;
}

ApplicationContainer
NrTrafficManager::GetDlServerApps() const
{
    return m_dlServerApps;
}

ApplicationContainer
NrTrafficManager::GetDlClientApps() const
{
    return m_dlClientApps;
}

ApplicationContainer
NrTrafficManager::GetUlServerApps() const
{
    return m_ulServerApps;
}

ApplicationContainer
NrTrafficManager::GetUlClientApps() const
{
    return m_ulClientApps;
}

bool
NrTrafficManager::IsInstalled() const
{
    return m_installed;
}

} // namespace ns3