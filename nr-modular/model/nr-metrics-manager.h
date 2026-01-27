/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

 /**
  * Contains class NrMetricsManager for managing metrics collection
  * Class responsibilities:
  * - Enable flow monitor
  * - Collect performance statistics
  * - Track throughput, delay, loss
  * CURRENT STATUS: STUB IMPLEMENTATION
  * Class methods and attributes:
  * - Public:
  *   - SetConfig: Set the simulation configuration.
  *   - EnableMetrics: Enable metrics collection between gNB and UE nodes.
  *   - CollectFinalMetrics: Collect final metrics at simulation end.    
  * - Protected:
  *   - DoDispose: Clean up resources.
  * - Private:
  *   - m_config: Pointer to the simulation configuration.
  *   - m_enabled: Flag indicating if metrics collection is enabled.
  */

 #ifndef NR_METRICS_MANAGER_H
 #define NR_METRICS_MANAGER_H

 #include "ns3/object.h"
 #include "ns3/ptr.h"
 #include "ns3/node-container.h"
 #include "utils/nr-sim-config.h"
 namespace ns3
 {
    class NrSimConfig; 

    /**
     * @brief Manager for metrics collection
     * 
     * Responsibilities:
     * - Enable flow monitor
     * - Collect performance statistics
     * - Track throughput, delay, loss
     * 
     * CURRENT STATUS: STUB IMPLEMENTATION
     * - Does nothing yet
     * - TODO: Implement FlowMonitor integration
     * - TODO: Collect PHY/MAC/RLC traces
     * - TODO: Calculate KPIs
     */
    class NrMetricsManager : public Object
    {
    public:
        static TypeId GetTypeId (void);
        NrMetricsManager ();
        virtual ~NrMetricsManager ();

        void SetConfig (Ptr<NrSimConfig> config);

        /**
         * @brief Enable metrics collection
         * @param gnbNodes gNB nodes
         * @param ueNodes UE nodes
         * 
         * STUB: Currently does nothing
         */
        void EnableMetrics(const NodeContainer& gnbNodes, 
            const NodeContainer& ueNodes);
        
        /**
         * @brief Collect final metrics
         * 
         * STUB: Currently does nothing
         */
        void CollectFinalMetrics();

    protected:
        virtual void DoDispose() override;
    private:
        Ptr<NrSimConfig> m_config; //!< Simulation configuration
        bool m_enabled;            //!< Metrics enabled flag
    };
    
 } // namespace ns3

 #endif // NR_METRICS_MANAGER_H