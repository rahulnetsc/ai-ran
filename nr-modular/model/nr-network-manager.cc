/*
 * Copyright (c) 2026 ARTPARK
 *
 * NR Network Manager Implementation
 * CORRECTED VERSION - Based on cttc-nr-demo.cc and cttc-3gpp-channel-simple-ran.cc
 */

#include "nr-network-manager.h"
#include "utils/nr-sim-config.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/ideal-beamforming-helper.h"
#include "ns3/cc-bwp-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/nr-channel-helper.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/eps-bearer.h"
#include "ns3/ideal-beamforming-algorithm.h"
#include "ns3/nr-eps-bearer.h"
#include "ns3/epc-tft.h"

#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/config.h"
#include "ns3/nr-ue-rrc.h"


#include <iostream>
#include <utility>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrNetworkManager");
NS_OBJECT_ENSURE_REGISTERED(NrNetworkManager);

TypeId
NrNetworkManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrNetworkManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrNetworkManager>();
    return tid;
}

NrNetworkManager::NrNetworkManager()
    : m_config(nullptr),
      m_epcHelper(nullptr),       // ✅ Now matches header order
      m_nrHelper(nullptr),        // ✅ Now matches header order
      m_channelHelper(nullptr),
      m_setup(false),
      m_installed(false)
{
    NS_LOG_FUNCTION(this);
}

NrNetworkManager::~NrNetworkManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrNetworkManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_config = nullptr;
    m_nrHelper = nullptr;
    m_epcHelper = nullptr;
    m_channelHelper = nullptr;
    m_gnbDevices = NetDeviceContainer();
    m_ueDevices = NetDeviceContainer();
    Object::DoDispose();
}

void
NrNetworkManager::SetConfig(const Ptr<NrSimConfig>& config)
{
    NS_LOG_FUNCTION(this << config);
    NS_ABORT_MSG_IF(config == nullptr, "NrNetworkManager: config cannot be null");
    m_config = config;
}

void
NrNetworkManager::SetupNrInfrastructure(const NodeContainer& gnbNodes,
                                   const NodeContainer& ueNodes)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(m_config == nullptr, "Config must be set before setup");
    NS_ABORT_MSG_IF(m_setup, "NR infrastructure already setup");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Setting up 5G NR infrastructure" << std::endl;
    std::cout << "========================================" << std::endl;

    // =================================================================
    // STEP 1: Create ALL helpers (from cttc-nr-demo.cc lines 255-261)
    // =================================================================
    std::cout << "Creating NR helpers..." << std::endl;
    
    m_epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    m_nrHelper = CreateObject<NrHelper>();
    m_channelHelper = CreateObject<NrChannelHelper>();  // ✅ CORRECT: Create early!
    
    // Connect helpers
    m_nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    m_nrHelper->SetEpcHelper(m_epcHelper);
    // m_nrHelper->SetChannelHelper(m_channelHelper);
    std::cout << "  ✓ Channel helper set in NR helper" << std::endl;

    
    Ptr<Node> pgw = m_epcHelper->GetPgwNode();
    
    std::cout << "  ✓ EPC created (PGW node ID: " << pgw->GetId() << ")" << std::endl;
    std::cout << "  ✓ NR Helper created" << std::endl;
    std::cout << "  ✓ Channel Helper created" << std::endl;

    // =================================================================
    // STEP 2: Configure Channel FIRST (before creating bands!)
    // From cttc-nr-demo.cc lines 306-313
    // =================================================================
    std::cout << "\nConfiguring channel..." << std::endl;
    
    std::string scenario = m_config->channel.propagationModel;
    double frequency = m_config->channel.frequency;
    double bandwidth = m_config->channel.bandwidth;
    
    std::cout << "  Scenario: " << scenario << std::endl;
    std::cout << "  Frequency: " << (frequency / 1e9) << " GHz" << std::endl;
    std::cout << "  Bandwidth: " << (bandwidth / 1e6) << " MHz" << std::endl;

    // ✅ CORRECT ORDER: ConfigureFactories BEFORE creating bands
    m_channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    
    // Optional: Set channel attributes (from examples)
    // m_channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    // m_channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    
    std::cout << "  ✓ Channel factories configured" << std::endl;

    // =================================================================
    // STEP 3: Create Operation Band (from cttc-nr-demo.cc lines 270-281)
    // =================================================================
    std::cout << "\nCreating operation band..." << std::endl;
    
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    
    CcBwpCreator::SimpleOperationBandConf bandConf(frequency, bandwidth, numCcPerBand);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    
    std::cout << "  ✓ Operation band created" << std::endl;
    
    // =================================================================
    // STEP 4: Assign Channels to Bands (from cttc-nr-demo.cc line 320 or 326)
    // =================================================================
    std::cout << "\nAssigning channel to band..." << std::endl;
    
    // ✅ CORRECT: Assign AFTER ConfigureFactories
    m_channelHelper->AssignChannelsToBands({band});
    
    std::cout << "  ✓ Channel assigned to band" << std::endl;

    
    // =================================================================
    // STEP 5: Get BWPs (from cttc-nr-demo.cc line 322 or 327)
    // =================================================================
    std::cout << "\nRetrieving BWPs..." << std::endl;
    
    m_allBwps = CcBwpCreator::GetAllBwps({band});
    
    std::cout << "  ✓ BWPs retrieved: " << m_allBwps.size() << std::endl;

    // =================================================================
    // STEP 6: Configure Beamforming (from cttc-nr-demo.cc lines 354-356)
    // =================================================================
    std::cout << "\nConfiguring beamforming..." << std::endl;
    
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));
    
    std::cout << "  ✓ Direct path beamforming enabled" << std::endl;

    // =================================================================
    // STEP 7: Configure Antennas (from cttc-nr-demo.cc lines 361-371)
    // =================================================================
    std::cout << "\nConfiguring antennas..." << std::endl;

    // UE antennas: 2x4 later 1x2 now for speed
    m_nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    m_nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(2));
    m_nrHelper->SetUeAntennaAttribute("AntennaElement",
                                      PointerValue(CreateObject<IsotropicAntennaModel>()));

    // gNB antennas: 4x8 (or 8x8 from simple-ran example) later 2x4 now for speed
    m_nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    m_nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(4));
    m_nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                       PointerValue(CreateObject<IsotropicAntennaModel>()));

    // std::cout << "  ✓ UE antenna: 2x4" << std::endl;
    // std::cout << "  ✓ gNB antenna: 4x8" << std::endl;
    // Display actual antenna config

    m_setup = true;

    NS_LOG_FUNCTION(this << gnbNodes.GetN() << ueNodes.GetN());
    NS_ABORT_MSG_IF(!m_setup, "Must call SetupNrInfrastructure() first");
    NS_ABORT_MSG_IF(m_installed, "NR devices already installed");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Installing NR devices" << std::endl;
    std::cout << "========================================" << std::endl;

    // =================================================================
    // Set HANDOVER PARAMETERS (if needed in future)
    // =================================================================
    std::cout << "\nSetting handover parameters..." << std::endl;
    m_nrHelper->SetHandoverAlgorithmType ("ns3::NrA3RsrpHandoverAlgorithm");
    m_nrHelper->SetHandoverAlgorithmAttribute ("Hysteresis",
                                                DoubleValue (3.0));
    m_nrHelper->SetHandoverAlgorithmAttribute ("TimeToTrigger",
                                                TimeValue (MilliSeconds (256)));
    std::cout << "  ✓ Handover algorithm configured" << std::endl;
    // =================================================================
    // STEP 8: Install gNB and UE devices
    // =================================================================
    // From cttc-nr-demo.cc lines 407-410
    std::cout << "Installing gNB devices..." << std::endl;
    m_gnbDevices = m_nrHelper->InstallGnbDevice(gnbNodes, m_allBwps);
    std::cout << "  ✓ " << m_gnbDevices.GetN() << " gNB devices installed" << std::endl;
   
    std::cout << "\nInstalling UE devices..." << std::endl;
    m_ueDevices = m_nrHelper->InstallUeDevice(ueNodes, m_allBwps);
    std::cout << "  ✓ " << m_ueDevices.GetN() << " UE devices installed" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "NR infrastructure setup complete!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    m_installed = true;
}


void
NrNetworkManager::AssignIpAddresses(const NodeContainer& ueNodes)
{
    NS_LOG_FUNCTION(this << ueNodes.GetN());
    NS_ABORT_MSG_IF(!m_installed, "Must call InstallNrDevices() first");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Assigning IP addresses" << std::endl;
    std::cout << "========================================" << std::endl;

    // From cttc-nr-demo.cc lines 443-450
    std::cout << "Installing internet stack..." << std::endl;
    
    InternetStackHelper internet;
    internet.Install(ueNodes);
    
    std::cout << "  ✓ Internet stack installed on " << ueNodes.GetN() << " UEs" << std::endl;

    std::cout << "\nAssigning IP addresses..." << std::endl;
    
    m_ueIpInterfaces = m_epcHelper->AssignUeIpv4Address(NetDeviceContainer(m_ueDevices));
    
    std::cout << "  ✓ " << m_ueIpInterfaces.GetN() << " IP addresses assigned" << std::endl;
    
    // Show sample IPs
    uint32_t samplesToShow = std::min(3u, (uint32_t)m_ueIpInterfaces.GetN());
    for (uint32_t i = 0; i < samplesToShow; ++i)
    {
        std::cout << "    UE " << i << ": " << m_ueIpInterfaces.GetAddress(i, 0) << std::endl;
    }
    if (m_ueIpInterfaces.GetN() > 3)
    {
        std::cout << "    ... (" << (m_ueIpInterfaces.GetN() - 3) << " more)" << std::endl;
    }

    std::cout << "========================================\n" << std::endl;

    
}

void NrNetworkManager::AttachUes(Ptr<NrHelper> nrHelper, NetDeviceContainer ueDevices, NetDeviceContainer gnbDevices)
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_installed, "Must call SetupNrInfrastructure() first");

    std::cout << "\n========================================" << std::endl;
    std::cout << "Attaching UEs to closest gNBs" << std::endl;
    std::cout << "========================================" << std::endl;

    // ADD DEBUG PRINTS:
    for (uint32_t i = 0; i < ueDevices.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueDevices.Get(i)->GetNode();
        Ptr<MobilityModel> mob = ueNode->GetObject<MobilityModel>();
        Vector pos = mob->GetPosition();
        std::cout << "  UE " << i << " position: (" 
                  << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }
    for (uint32_t j = 0; j < gnbDevices.GetN(); ++j)
    {
        Ptr<Node> gnbNode = gnbDevices.Get(j)->GetNode();
        Ptr<MobilityModel> mob = gnbNode->GetObject<MobilityModel>();
        Vector pos = mob->GetPosition();
        std::cout << "  gNB " << j << " position: (" 
                  << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }
    nrHelper->AttachToClosestGnb(ueDevices, gnbDevices);

    // Optional: Ensure the RLC reassembly timer is long enough to handle handover jitter
    // Config::SetDefault("ns3::NrRlcAm::ReassemblyTimer", TimeValue(MilliSeconds(200)));
    
    std::cout << "  ✓ " << ueDevices.GetN() << " UEs attached" << std::endl;
    std::cout << "========================================\n" << std::endl;
}


// ================================================================
// HANDOVER FUNCTIONALITY (if needed in future)
// ================================================================
void
NrNetworkManager::EnableHandoverTracing(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    NS_ABORT_MSG_IF(!m_installed, "Must call SetupNrInfrastructure() first");
    
    m_enableHandoverTracing = enable;
    m_totalHandovers = 0;
    
    if (!enable)
        return;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Enabling handover tracing" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Hook into RRC traces for each UE
    for (uint32_t i = 0; i < m_ueDevices.GetN(); ++i)
    {
        Ptr<NetDevice> ueDevice = m_ueDevices.Get(i);
        Ptr<NrUeNetDevice> nrUeDevice = DynamicCast<NrUeNetDevice>(ueDevice);
        
        // Get RRC layer
        Ptr<NrUeRrc> rrc = nrUeDevice->GetRrc();
        
        // Connect to handover traces
        std::ostringstream path;
        path << "/NodeList/" << ueDevice->GetNode()->GetId()
             << "/DeviceList/" << ueDevice->GetIfIndex()
             << "/$ns3::NrUeNetDevice/NrUeRrc/";
        
        // ✅ NEW: Track initial connection establishment
        Config::Connect(path.str() + "ConnectionEstablished",
                       MakeCallback(&NrNetworkManager::NotifyConnectionEstablished, this));
        
        // HandoverStart trace
        Config::Connect(path.str() + "HandoverStart",
                       MakeCallback(&NrNetworkManager::NotifyHandoverStart, this));
        
        // HandoverEndOk trace
        Config::Connect(path.str() + "HandoverEndOk",
                       MakeCallback(&NrNetworkManager::NotifyHandoverEndOk, this));
        
        // HandoverEndError trace (optional - for debugging)
        Config::Connect(path.str() + "HandoverEndError",
                       MakeCallback(&NrNetworkManager::NotifyHandoverEndError, this));
        
        // Initialize tracking map
        uint64_t imsi = rrc->GetImsi();
        m_imsiToUeIndexMap[imsi] = i; 
        m_ueToGnbMap[imsi] = 0; // Will be updated on connection/handover
        m_handoverCountPerUe[i] = 0;
    }
    
    std::cout << "  ✓ Handover tracing enabled for " << m_ueDevices.GetN() << " UEs" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void
NrNetworkManager::NotifyConnectionEstablished(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    NS_LOG_FUNCTION(this << context << imsi << cellId << rnti);
    
    uint32_t ueIndex = ImsiToUeIndex(imsi);
    double now = Simulator::Now().GetSeconds();
    
    // Update serving gNB map
    m_ueToGnbMap[imsi] = cellId;
    
    std::cout << "[ATTACH] t=" << std::fixed << std::setprecision(3) << now << "s "
              << "UE " << ueIndex << " (IMSI:" << imsi << ") "
              << "✓ Connected to gNB " << cellId << " (Cell ID: " << cellId << ")" << std::endl;
}

void 
NrNetworkManager::NotifyHandoverStart(std::string context, uint64_t imsi, 
                        uint16_t sourceCellId, uint16_t rnti, uint16_t targetCellId)
{
    uint32_t ueIndex = ImsiToUeIndex(imsi);
    double now = Simulator::Now().GetSeconds();
    
    std::cout << "[HANDOVER] t=" << std::fixed << std::setprecision(3) << now << "s "
              << "UE " << ueIndex << " (IMSI:" << imsi << ") "
              << "starting handover: gNB " << sourceCellId 
              << " → gNB " << targetCellId << std::endl;
    
    m_totalHandovers++;
    m_handoverCountPerUe[ueIndex]++;
}

void
NrNetworkManager::NotifyHandoverEndOk(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    uint32_t ueIndex = ImsiToUeIndex(imsi);
    double now = Simulator::Now().GetSeconds();
    
    // Update serving gNB
    m_ueToGnbMap[imsi] = cellId;
    
    std::cout << "[HANDOVER] t=" << std::fixed << std::setprecision(3) << now << "s "
              << "UE " << ueIndex << " (IMSI:" << imsi << ") "
              << "✓ handover SUCCESS to gNB " << cellId << std::endl;
}

void
NrNetworkManager::NotifyHandoverEndError(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    uint32_t ueIndex = ImsiToUeIndex(imsi);
    double now = Simulator::Now().GetSeconds();
    
    std::cout << "[HANDOVER] t=" << std::fixed << std::setprecision(3) << now << "s "
              << "UE " << ueIndex << " (IMSI:" << imsi << ") "
              << "✗ handover FAILED to gNB " << cellId << std::endl;
}

uint32_t
NrNetworkManager::ImsiToUeIndex(uint64_t imsi) const
{
    auto it = m_imsiToUeIndexMap.find(imsi);
    if (it != m_imsiToUeIndexMap.end())
        return it->second;
    
    NS_LOG_WARN("IMSI " << imsi << " not in map!");
    return static_cast<uint32_t>(imsi - 1);
}

uint32_t
NrNetworkManager::GetTotalHandovers() const
{
    return m_totalHandovers;
}

uint32_t
NrNetworkManager::GetUeHandoverCount(uint32_t ueId) const
{
    auto it = m_handoverCountPerUe.find(ueId);
    if (it != m_handoverCountPerUe.end())
        return it->second;
    return 0;
}

uint16_t
NrNetworkManager::GetServingGnb(uint32_t ueId) const
{
    // Get IMSI for this UE (IMSI = ueId + 1)
    uint64_t imsi = ueId + 1;
    
    // Try to get from tracking map first
    auto it = m_ueToGnbMap.find(imsi);
    if (it != m_ueToGnbMap.end() && it->second != 0)
    {
        return it->second;
    }
    
    // ✅ NEW: If not in map or is 0, query actual serving cell from RRC
    if (ueId < m_ueDevices.GetN())
    {
        Ptr<NrUeNetDevice> nrUeDevice = DynamicCast<NrUeNetDevice>(m_ueDevices.Get(ueId));
        if (nrUeDevice)
        {
            Ptr<NrUeRrc> rrc = nrUeDevice->GetRrc();
            if (rrc)
            {
                // Get cell ID from RRC state
                uint16_t cellId = rrc->GetCellId();
                return cellId;
            }
        }
    }
    
    return 0; // Not attached
}

void
NrNetworkManager::PrintAttachmentStatus() const
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Current UE Attachment Status" << std::endl;
    std::cout << "========================================" << std::endl;
    
    for (uint32_t i = 0; i < m_ueDevices.GetN(); ++i)
    {
        Ptr<NrUeNetDevice> nrUeDevice = DynamicCast<NrUeNetDevice>(m_ueDevices.Get(i));
        if (nrUeDevice)
        {
            Ptr<NrUeRrc> rrc = nrUeDevice->GetRrc();
            if (rrc)
            {
                uint16_t cellId = rrc->GetCellId();
                uint64_t imsi = rrc->GetImsi();
                
                // Get UE position
                Ptr<Node> ueNode = nrUeDevice->GetNode();
                Ptr<MobilityModel> mob = ueNode->GetObject<MobilityModel>();
                Vector pos = mob->GetPosition();
                
                std::cout << "  UE " << i << " (IMSI:" << imsi << ")"
                          << " @ (" << std::fixed << std::setprecision(0)
                          << pos.x << "," << pos.y << ")"
                          << " → Cell " << cellId 
                          << " (Handovers: " << GetUeHandoverCount(i) << ")"
                          << std::endl;
            }
        }
    }
    
    std::cout << "========================================\n" << std::endl;
}

// ================================================================

bool
NrNetworkManager::IsSetup() const
{
    return m_setup;
}

bool
NrNetworkManager::IsInstalled() const
{
    return m_installed;
}

NetDeviceContainer
NrNetworkManager::GetGnbDevices() const
{
    return m_gnbDevices;
}

NetDeviceContainer
NrNetworkManager::GetUeDevices() const
{
    return m_ueDevices;
}

Ptr<NrHelper>
NrNetworkManager::GetNrHelper() const
{
    return m_nrHelper;
}

Ptr<NrPointToPointEpcHelper>
NrNetworkManager::GetEpcHelper() const
{
    return m_epcHelper;
}

Ipv4InterfaceContainer
NrNetworkManager::GetUeIpInterfaces() const
{
    return m_ueIpInterfaces;
}

} // namespace ns3