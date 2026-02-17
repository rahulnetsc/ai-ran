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

#include <sstream>
#include <iostream>
#include <iomanip>   // std::setprecision, std::fixed

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
            std::cout << "  \n[APP RX] " << context << " - Max prints reached. Silencing...\n" << std::endl;
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
            std::cout << "  \n[L3] " << context << " - Max prints reached. Silencing...\n" << std::endl;
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
      m_enableRealTimeMonitoring(false),
      m_monitoringInterval(1.0),
      m_trafficStartTime(0.0),  // Set properly in InstallTraffic() from config
      m_lastSampleTime(Seconds(0)),  // ADD THIS
      m_installed(false),
      m_metricsCollected(false),
      m_remoteHost(nullptr)
{
    NS_LOG_FUNCTION(this);
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
    // Read startTime from config; fall back to 0.5s (safe minimum after RRC attach ~20ms)
    double startTime = (m_config->traffic.startTime > 0.0)
                        ? m_config->traffic.startTime
                        : 0.5;
    double stopTime = m_config->simDuration;
    NS_ABORT_MSG_IF(stopTime <= startTime + 0.5,
        "simDuration (" << stopTime << "s) must be > startTime+0.5s (" 
        << (startTime + 0.5) << "s). Increase simDuration in config.");

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

        dlClient.SetConstantRate(DataRate(dlRate));
        dlClient.SetAttribute("PacketSize", UintegerValue(dlPacketSize));
        
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
                            
        ulClient.SetConstantRate(DataRate(ulRate));
        ulClient.SetAttribute("PacketSize", UintegerValue(ulPacketSize));
        
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


    m_trafficStartTime = startTime;  // Set from config (traffic.startTime)

    // Initialize metrics map
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        PerUeMetrics metrics;
        metrics.ueId = i;
        m_ueMetrics[i] = metrics;
    }

    std::cout << "✓ Real-time monitoring initialized for " << ueNodes.GetN() << " UEs" << std::endl;

    std::cout << "[INSTALL] DL Sinks: " << m_dlServerApps.GetN() 
          << ", UE Metrics: " << m_ueMetrics.size() << std::endl;
          
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

    // Setup comprehensive tracing if enabled
    if (!m_config->logTraffic)
    {
        std::cout << "\nTraffic logging disabled in config. Skipping tracing setup." << std::endl;
        return;  // m_installed already set to true above
    }
    else
    {
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
    }

    

    if (m_config->logTraffic)
    {
        std::cout << "\n⚠️  IMPORTANT: Watch for RX traces during simulation!" << std::endl;
        std::cout << "Expected traces:" << std::endl;
        std::cout << "  - Remote_RX: Uplink packets arriving at remote host" << std::endl;
        std::cout << "  - UE*_RX: Downlink packets arriving at UEs" << std::endl;
        std::cout << "  - *_DROP: Any dropped packets (investigate if seen!)" << std::endl;
        std::cout << "  - DL_Sink_UE_*: Successful app-layer DL reception" << std::endl;
        std::cout << "  - UL_Sink_Remote_*: Successful app-layer UL reception\n" << std::endl;
    }

    
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
    // Sources start at (m_trafficStartTime + 0.5s), sinks start at m_trafficStartTime.
    // Use source start time as the actual traffic begin point.
    m_trafficDuration = m_config->simDuration - (m_trafficStartTime + 0.5);
    NS_ABORT_MSG_IF(m_trafficDuration <= 0,
        "trafficDuration <= 0! simDuration=" << m_config->simDuration 
        << " startTime=" << m_trafficStartTime << ". Increase simDuration.");

    
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
NrTrafficManager::PrintMetricsSummary() const
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_metricsCollected, "Must call CollectMetrics() first");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Traffic Metrics Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    
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

    // Aggregate metrics
    std::cout << "\n--- Aggregate Metrics ---" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total DL Throughput: " << m_aggregateMetrics.totalDlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Total UL Throughput: " << m_aggregateMetrics.totalUlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg DL Throughput/UE: " << m_aggregateMetrics.avgDlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg UL Throughput/UE: " << m_aggregateMetrics.avgUlThroughputMbps << " Mbps" << std::endl;
    std::cout << "Avg System Delay: " << m_aggregateMetrics.avgSystemDelayMs << " ms" << std::endl;
    std::cout << "Overall Packet Loss: " << (m_aggregateMetrics.overallPacketLossRate * 100.0) << "%" << std::endl;

    std::cout << "========================================\n" << std::endl;
}


void
NrTrafficManager::EnableRealTimeMonitoring(double interval)
{
    NS_LOG_FUNCTION(this << interval);
    
    if (!m_installed)
    {
        NS_LOG_WARN("Cannot enable monitoring before traffic is installed");
        return;
    }
    
    m_enableRealTimeMonitoring = true;
    m_monitoringInterval = interval;
    
    // Initialize sampling state
    uint32_t numUes = m_ueMetrics.size();
    m_lastDlRxBytes.resize(numUes, 0);
    m_lastUlRxBytes.resize(numUes, 0);
    m_lastSampleTime = Simulator::Now();
    
    // Initialize previous metrics for delta calculation
    m_previousMetrics.clear();
    for (auto& pair : m_ueMetrics)
    {
        m_previousMetrics[pair.first] = pair.second;
    }
    
    // Schedule first monitoring event
    m_monitoringEvent = Simulator::Schedule(
        Seconds(interval),
        &NrTrafficManager::MonitorFlows,
        this
    );
    
    NS_LOG_INFO("Real-time monitoring enabled, interval=" << interval << "s");
    std::cout << "✓ Real-time traffic monitoring enabled (PacketSink sampling)" << std::endl;
    std::cout << "  Interval: " << interval << " seconds" << std::endl;
    std::cout << "  Monitoring " << numUes << " UEs" << std::endl;
}

void
NrTrafficManager::DisableRealTimeMonitoring()
{
    NS_LOG_FUNCTION(this);
    
    m_enableRealTimeMonitoring = false;
    
    if (m_monitoringEvent.IsPending())
    {
        Simulator::Cancel(m_monitoringEvent);
    }
    
    NS_LOG_INFO("Real-time monitoring disabled");
}

void
NrTrafficManager::MonitorFlows()
{
    NS_LOG_FUNCTION(this);
    
    // OPTIONAL DEBUG: Remove after testing
    static int callCount = 0;
    if ((callCount < 5) && m_config->debug.enableDebugLogs)
    {
        std::cout << "[MONITOR] Sample #" << ++callCount 
                  << " at t=" << Simulator::Now().GetSeconds() << "s" << std::endl;
    }
    
    if (!m_enableRealTimeMonitoring)
    {
        return;
    }
    
    // Sample PacketSink statistics
    ProcessFlowMonitorStats();
    
    // OPTIONAL DEBUG: Show first few samples
    if ((callCount <= 5) && m_config->debug.enableDebugLogs)
    {
        if ((m_ueMetrics.size() > 0) && (m_config->debug.enableDebugLogs))
        {
            auto& ue0 = m_ueMetrics[0];
            std::cout << "  UE 0: DL=" << ue0.dlThroughputMbps << " Mbps, "
                      << "UL=" << ue0.ulThroughputMbps << " Mbps, "
                      << "Loss=" << (ue0.dlPacketLossRate * 100.0) << "%" << std::endl;
        }
    }
    
    // Schedule next monitoring
    if (m_enableRealTimeMonitoring)
    {
        m_monitoringEvent = Simulator::Schedule(
            Seconds(m_monitoringInterval),
            &NrTrafficManager::MonitorFlows,
            this
        );
    }
}



void
NrTrafficManager::ProcessFlowMonitorStats()
{
    double now = Simulator::Now().GetSeconds();

    static int callNum = 0;
    if (callNum++ < 3) std::cout << "[SAMPLE] t=" << Simulator::Now().GetSeconds() 
        << "s, sinks=" << m_dlServerApps.GetN() << ", metrics=" << m_ueMetrics.size() << std::endl;
    
    // Wait for traffic to start
    if (now < m_trafficStartTime)
        return;
    
    double timeSinceStart = now - m_trafficStartTime;
    
    // Sample downlink (PacketSinks on UEs)
    for (uint32_t i = 0; i < m_dlServerApps.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(m_dlServerApps.Get(i));
        if (!sink || i >= m_ueMetrics.size())
            continue;
        
        uint64_t rxBytes = sink->GetTotalRx();
        
        // Throughput = bytes / time
        m_ueMetrics[i].dlThroughputMbps = (rxBytes * 8.0) / (timeSinceStart * 1e6);
        m_ueMetrics[i].dlRxBytes = rxBytes;
        m_ueMetrics[i].dlRxPackets = rxBytes / m_config->traffic.packetSizeDl;
        
        // Expected packets
        double expectedBitsPerSec = m_config->traffic.udpRateDl * 1e6;
        uint64_t expectedTx = (expectedBitsPerSec * timeSinceStart) / 
                             (m_config->traffic.packetSizeDl * 8);
        
        m_ueMetrics[i].dlTxPackets = expectedTx;
        m_ueMetrics[i].dlLostPackets = (expectedTx > m_ueMetrics[i].dlRxPackets) ?
                                        (expectedTx - m_ueMetrics[i].dlRxPackets) : 0;
        
        if (expectedTx > 0)
            m_ueMetrics[i].dlPacketLossRate = 
                (double)m_ueMetrics[i].dlLostPackets / expectedTx;
    }
    
    // Sample uplink (PacketSinks on remote host)
    for (uint32_t i = 0; i < m_ulServerApps.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(m_ulServerApps.Get(i));
        if (!sink || i >= m_ueMetrics.size())
            continue;
        
        uint64_t rxBytes = sink->GetTotalRx();
        
        m_ueMetrics[i].ulThroughputMbps = (rxBytes * 8.0) / (timeSinceStart * 1e6);
        m_ueMetrics[i].ulRxBytes = rxBytes;
        m_ueMetrics[i].ulRxPackets = rxBytes / m_config->traffic.packetSizeUl;
        
        double expectedBitsPerSec = m_config->traffic.udpRateUl * 1e6;
        uint64_t expectedTx = (expectedBitsPerSec * timeSinceStart) / 
                             (m_config->traffic.packetSizeUl * 8);
        
        m_ueMetrics[i].ulTxPackets = expectedTx;
        m_ueMetrics[i].ulLostPackets = (expectedTx > m_ueMetrics[i].ulRxPackets) ?
                                        (expectedTx - m_ueMetrics[i].ulRxPackets) : 0;
        
        if (expectedTx > 0)
            m_ueMetrics[i].ulPacketLossRate = 
                (double)m_ueMetrics[i].ulLostPackets / expectedTx;
    }
    
    // Compute totals
    ComputeAggregateMetrics();
}

void
NrTrafficManager::ComputeAggregateMetrics()
{
    m_aggregateMetrics.totalDlThroughputMbps = 0.0;
    m_aggregateMetrics.totalUlThroughputMbps = 0.0;
    m_aggregateMetrics.totalPacketsSent = 0;
    m_aggregateMetrics.totalPacketsReceived = 0;
    m_aggregateMetrics.totalPacketsLost = 0;
    
    m_aggregateMetrics.numUes = m_ueMetrics.size();
    // Only print once
    static bool hasPrintedNumUes = false;
    if (m_config->debug.enableDebugLogs && !hasPrintedNumUes)
    {
        std::cout << "Number of UEs: " << m_aggregateMetrics.numUes << std::endl;
        hasPrintedNumUes = true;
    }

    double totalDelaySum = 0.0;
    uint32_t totalDelayCount = 0;
    
    for (const auto& pair : m_ueMetrics)
    {
        const PerUeMetrics& ueMetrics = pair.second;
        
        // Throughput
        m_aggregateMetrics.totalDlThroughputMbps += ueMetrics.dlThroughputMbps;
        m_aggregateMetrics.totalUlThroughputMbps += ueMetrics.ulThroughputMbps;
        
        // Packets
        m_aggregateMetrics.totalPacketsSent += ueMetrics.dlTxPackets + ueMetrics.ulTxPackets;
        m_aggregateMetrics.totalPacketsReceived += ueMetrics.dlRxPackets + ueMetrics.ulRxPackets;
        m_aggregateMetrics.totalPacketsLost += ueMetrics.dlLostPackets + ueMetrics.ulLostPackets;
        
        // Delay
        if (ueMetrics.dlRxPackets > 0)
        {
            totalDelaySum += ueMetrics.dlAvgDelayMs * ueMetrics.dlRxPackets;
            totalDelayCount += ueMetrics.dlRxPackets;
        }
        if (ueMetrics.ulRxPackets > 0)
        {
            totalDelaySum += ueMetrics.ulAvgDelayMs * ueMetrics.ulRxPackets;
            totalDelayCount += ueMetrics.ulRxPackets;
        }
    }
    
    // Averages
    if (m_aggregateMetrics.numUes > 0)
    {
        m_aggregateMetrics.avgDlThroughputMbps = m_aggregateMetrics.totalDlThroughputMbps / m_aggregateMetrics.numUes;
        m_aggregateMetrics.avgUlThroughputMbps = m_aggregateMetrics.totalUlThroughputMbps / m_aggregateMetrics.numUes;
    }
    
    if (totalDelayCount > 0)
    {
        m_aggregateMetrics.avgSystemDelayMs = totalDelaySum / totalDelayCount;
    }
    
    if (m_aggregateMetrics.totalPacketsSent > 0)
    {
        m_aggregateMetrics.overallPacketLossRate = 
            static_cast<double>(m_aggregateMetrics.totalPacketsLost) / m_aggregateMetrics.totalPacketsSent;
    }

    if (m_config->debug.enableDebugLogs)
    {
        std::cout << "  ✓ Aggregate metrics computed" << std::endl;
        std::cout << "[RESULT] DL=" << m_aggregateMetrics.totalDlThroughputMbps << " Mbps" << std::endl;
        std::cout << "[RESULT] UL=" << m_aggregateMetrics.totalUlThroughputMbps << " Mbps" << std::endl;
        std::cout << "[RESULT] Packet Loss Rate=" << m_aggregateMetrics.overallPacketLossRate << std::endl;
        std::cout << "[RESULT] Avg System Delay=" << m_aggregateMetrics.avgSystemDelayMs << " ms" << std::endl;
    }
    
}

// ========================================================================
// GETTER METHODS (if not already implemented)
// ========================================================================

PerUeMetrics
NrTrafficManager::GetUeMetrics(uint32_t ueId) const
{
    auto it = m_ueMetrics.find(ueId);
    if (it != m_ueMetrics.end())
    {
        return it->second;
    }
    
    // Return empty metrics if not found
    return PerUeMetrics();
}

std::map<uint32_t, PerUeMetrics>
NrTrafficManager::GetAllUeMetrics() const
{
    return m_ueMetrics;
}

AggregateMetrics
NrTrafficManager::GetAggregateMetrics() const
{
    return m_aggregateMetrics;
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

bool
NrTrafficManager::IsCollected() const
{
    return m_metricsCollected;
}

} // namespace ns3