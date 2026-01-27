/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "nr-channel-manager.h"
#include "utils/nr-sim-config.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/nr-channel-helper.h"
#include "ns3/three-gpp-propagation-loss-model.h"

#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrChannelManager");
NS_OBJECT_ENSURE_REGISTERED(NrChannelManager);

TypeId
NrChannelManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrChannelManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrChannelManager>();
    return tid;
}

NrChannelManager::NrChannelManager()
    : m_config(nullptr),
      m_channelHelper(nullptr),
      m_configured(false),
      m_frequency(0.0),
      m_bandwidth(0.0),
      m_propagationModel(""),
      m_rayTracingEnabled(false),
      m_sionnaServerUrl(""),
      m_sceneFile("")
{
    NS_LOG_FUNCTION(this);
}

NrChannelManager::~NrChannelManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrChannelManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_config = nullptr;
    m_channelHelper = nullptr;
    Object::DoDispose();
}

void
NrChannelManager::SetConfig(const Ptr<NrSimConfig>& config)
{
    NS_LOG_FUNCTION(this << config);
    m_config = config;
    
    // Cache values from config for easy access
    m_frequency = m_config->channel.frequency;
    m_bandwidth = m_config->channel.bandwidth;
    m_propagationModel = m_config->channel.propagationModel;
}

void
NrChannelManager::ConfigureChannel(const NodeContainer& gnbNodes, const NodeContainer& ueNodes)
{
    NS_LOG_FUNCTION(this << gnbNodes.GetN() << ueNodes.GetN());
    NS_ABORT_MSG_IF(m_config == nullptr, "Config must be set before configuring channel!");

    std::cout << "Configuring 5G-LENA Channel Model..." << std::endl;
    std::cout << "  Scenario: " << m_propagationModel << std::endl;
    std::cout << "  Frequency: " << m_frequency / 1e9 << " GHz" << std::endl;

    // 1. Create the Channel Helper
    m_channelHelper = CreateObject<NrChannelHelper>();

    // 2. Set the 3GPP propagation scenario (UMa, UMi, etc.)
    m_channelHelper->SetAttribute("Scenario", StringValue(m_propagationModel));

    // 3. Configure the Spectrum and Pathloss models
    // We use the 3GPP TR 38.901 statistical models by default
    m_channelHelper->SetAttribute("PathlossModel", StringValue("ns3::ThreeGppUmaPathLossModel"));
    
    // 4. Enable Shadowing and Fast Fading if desired
    m_channelHelper->SetAttribute("ShadowingEnabled", BooleanValue(true));

    // 5. Setup the actual channel objects for the nodes
    // Note: In 5G-LENA, the actual channel assignment often happens 
    // during the SpectrumPhy installation in the helper.
    
    PrintChannelDetails();
    m_configured = true;
}

Ptr<NrChannelHelper>
NrChannelManager::GetChannelHelper() const
{
    return m_channelHelper;
}

bool
NrChannelManager::IsConfigured() const
{
    return m_configured;
}

double
NrChannelManager::GetFrequency() const
{
    return m_frequency;
}

double
NrChannelManager::GetBandwidth() const
{
    return m_bandwidth;
}

std::string
NrChannelManager::GetPropagationModel() const
{
    return m_propagationModel;
}

// =====================================================================
// SIONNA-RT INTEGRATION (FUTURE STUBS)
// =====================================================================

void
NrChannelManager::EnableSionnaRayTracing(const std::string& sionnaServerUrl,
                                        const std::string& sceneFile)
{
    NS_LOG_FUNCTION(this << sionnaServerUrl << sceneFile);
    m_rayTracingEnabled = true;
    m_sionnaServerUrl = sionnaServerUrl;
    m_sceneFile = sceneFile;
    
    NS_LOG_INFO("Sionna-RT Ray Tracing enabled (STUB). URL: " << m_sionnaServerUrl);
}

void
NrChannelManager::UpdateRayTracedChannel()
{
    if (!m_rayTracingEnabled) return;
    NS_LOG_FUNCTION(this);
    // Future: Trigger API call to Sionna-RT server to get new CIR
}

void
NrChannelManager::DisableSionnaRayTracing()
{
    NS_LOG_FUNCTION(this);
    m_rayTracingEnabled = false;
}

bool
NrChannelManager::IsRayTracingEnabled() const
{
    return m_rayTracingEnabled;
}

// =====================================================================
// PRIVATE HELPERS
// =====================================================================

std::string
NrChannelManager::GetScenarioString() const
{
    return m_propagationModel;
}

void
NrChannelManager::PrintChannelDetails() const
{
    std::cout << "  --- Channel Parameters ---" << std::endl;
    std::cout << "  Center Frequency: " << m_frequency << " Hz" << std::endl;
    std::cout << "  Bandwidth:        " << m_bandwidth / 1e6 << " MHz" << std::endl;
    std::cout << "  Model:            " << m_propagationModel << std::endl;
    std::cout << "  Ray-Tracing:      " << (m_rayTracingEnabled ? "Enabled" : "Disabled (3GPP Statistical)") << std::endl;
}

} // namespace ns3