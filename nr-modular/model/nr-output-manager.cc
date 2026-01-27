/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

 #include "nr-output-manager.h"

 #include "ns3/log.h"
 #include "ns3/abort.h"

 namespace ns3 {
    NS_LOG_COMPONENT_DEFINE ("NrOutputManager");
    NS_OBJECT_ENSURE_REGISTERED (NrOutputManager);

    TypeId
    NrOutputManager::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::NrOutputManager")
            .SetParent<Object> ()
            .SetGroupName("NrModular")
            .AddConstructor<NrOutputManager> ();
        return tid;     
    }

    NrOutputManager::NrOutputManager ()
        : m_config(nullptr),
          m_written(false)
    {
        NS_LOG_FUNCTION(this);
    }

    NrOutputManager::~NrOutputManager ()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    NrOutputManager::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        m_config = nullptr;
        Object::DoDispose();
    }

    void
    NrOutputManager::SetConfig(Ptr<NrSimConfig> config)
    {
        NS_LOG_FUNCTION(this);
        m_config = config;
        NS_LOG_INFO("Configuration set in NrOutputManager");
    }

    void 
    NrOutputManager::WriteResults()
    {
        NS_LOG_FUNCTION(this);
        
        NS_ABORT_MSG_IF(m_config == nullptr, "Config not set!");
        NS_ABORT_MSG_IF(m_written, "WriteResults() called twice!");
        
        NS_LOG_INFO("========================================");
        NS_LOG_INFO("NrOutputManager::WriteResults() - STUB");
        NS_LOG_INFO("Output File: " << m_config->outputFilePath);
        NS_LOG_INFO("WARNING: Results NOT actually written!");
        NS_LOG_INFO("========================================");
        
        NS_LOG_WARN("TODO: Write metrics to file");
        NS_LOG_WARN("TODO: Generate summary report");
        NS_LOG_WARN("TODO: Export in CSV/XML format");
        
        m_written = true;
    }
 } // namespace ns3