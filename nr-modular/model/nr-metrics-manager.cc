/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

/**
 * STUB: TO DO:
 * - Implement FlowMonitor integration
 * - Collect PHY/MAC/RLC traces
 * - Calculate KPIs
 */

#include "nr-metrics-manager.h"

#include "ns3/log.h"
#include "ns3/abort.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("NrMetricsManager");
    NS_OBJECT_ENSURE_REGISTERED(NrMetricsManager);

    TypeId
    NrMetricsManager::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::NrMetricsManager")
                                .SetParent<Object>()
                                .SetGroupName("NrModular")
                                .AddConstructor<NrMetricsManager>();
        return tid;
    }

    NrMetricsManager::NrMetricsManager() 
        : m_config(nullptr),
        m_enabled(false)
    {
        NS_LOG_FUNCTION(this);
    }

    NrMetricsManager::~NrMetricsManager()
    {
        NS_LOG_FUNCTION(this);
    }

    void 
    NrMetricsManager::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        m_config = nullptr;
        m_enabled = false;
        Object::DoDispose();
    }

    void
    NrMetricsManager::SetConfig(Ptr<NrSimConfig> config)
    {
        NS_LOG_FUNCTION(this);
        NS_ABORT_MSG_IF(config == nullptr, "Config pointer cannot be null");
        m_config = config;
    }

    void 
    NrMetricsManager::EnableMetrics(const NodeContainer& gnbNodes, 
        const NodeContainer& ueNodes)
    {
        NS_LOG_FUNCTION(this);
    
        NS_ABORT_MSG_IF(m_config == nullptr, "Config not set!");
        NS_ABORT_MSG_IF(m_enabled, "EnableMetrics() called twice!");
        
        NS_LOG_INFO("========================================");
        NS_LOG_INFO("NrMetricsManager::EnableMetrics() - STUB");
        NS_LOG_INFO("Flow Monitor: " << (m_config->enableFlowMonitor ? "Enabled" : "Disabled"));
        NS_LOG_INFO("WARNING: Metrics NOT actually enabled!");
        NS_LOG_INFO("========================================");
        
        NS_LOG_WARN("TODO: Implement FlowMonitor");
        NS_LOG_WARN("TODO: Enable PHY/MAC/RLC traces");
        NS_LOG_WARN("TODO: Set up metric callbacks");
        
        m_enabled = true;
    }

    void
    NrMetricsManager::CollectFinalMetrics()
    {
        NS_LOG_FUNCTION(this);
        
        if (!m_enabled)
        {
            NS_LOG_WARN("Metrics not enabled, nothing to collect");
            return;
        }
        
        NS_LOG_INFO("========================================");
        NS_LOG_INFO("NrMetricsManager::CollectFinalMetrics() - STUB");
        NS_LOG_INFO("WARNING: No metrics actually collected!");
        NS_LOG_INFO("========================================");
        
        NS_LOG_WARN("TODO: Collect FlowMonitor statistics");
        NS_LOG_WARN("TODO: Calculate throughput, delay, loss");
        NS_LOG_WARN("TODO: Generate performance reports");
    }

} // namespace ns3
