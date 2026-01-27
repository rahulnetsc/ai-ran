/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_CHANNEL_MANAGER_H
#define NR_CHANNEL_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node-container.h"
#include "ns3/nr-module.h"
#include "ns3/cc-bwp-helper.h"

namespace ns3
{

class NrSimConfig;

/**
 * @brief Manager for 5G-LENA channel configuration
 * 
 * FULL 5G-LENA IMPLEMENTATION:
 * - 3GPP TR 38.901 channel models (UMa, UMi, RMa, InH)
 * - NrChannelHelper integration
 * - Pathloss, shadowing, and fast fading
 * - Channel condition model (LOS/NLOS)
 * 
 * FUTURE ENHANCEMENTS:
 * - Sionna-RT ray-traced channel integration
 * - Real-time channel updates based on UE mobility
 */
class NrChannelManager : public Object
{
  public:
    static TypeId GetTypeId();

    NrChannelManager();
    ~NrChannelManager() override;

    /**
     * @brief Set simulation configuration
     * @param config Pointer to NrSimConfig object
     */
    void SetConfig(const Ptr<NrSimConfig>& config);

    /**
     * @brief Configure 5G-LENA channel using NrChannelHelper
     * @param nrHelper Pointer to NrHelper
     * @param band OperationBandInfo for the channel (NON-CONST REFERENCE!)
     * 
     * This method performs complete 3GPP channel setup:
     * 1. Creates NrChannelHelper
     * 2. Configures 3GPP TR 38.901 channel factories
     * 3. Sets propagation loss model (UMa/UMi/RMa/InH)
     * 4. Enables shadowing and fast fading
     * 5. Assigns channels to operation band
     */
    void ConfigureChannel(const NodeContainer& gnbNodes, const NodeContainer& ueNodes);

    /**
     * @brief Get the configured NrChannelHelper
     * @return Pointer to NrChannelHelper (null if not configured)
     */
    Ptr<NrChannelHelper> GetChannelHelper() const;

    /**
     * @brief Check if channel has been configured
     */
    bool IsConfigured() const;

    /**
     * @brief Get configured frequency (Hz)
     */
    double GetFrequency() const;

    /**
     * @brief Get configured bandwidth (Hz)
     */
    double GetBandwidth() const;

    /**
     * @brief Get propagation model name
     */
    std::string GetPropagationModel() const;

    // =====================================================================
    // FUTURE: Sionna-RT Integration (Real-time Ray-traced Channel)
    // =====================================================================

    /**
     * @brief Enable Sionna-RT ray-traced channel (FUTURE STUB)
     * 
     * Currently falls back to 3GPP statistical model.
     * Future implementation will provide real-time ray-tracing.
     */
    void EnableSionnaRayTracing(const std::string& sionnaServerUrl,
                                const std::string& sceneFile);

    /**
     * @brief Update ray-traced channel (FUTURE STUB)
     */
    void UpdateRayTracedChannel();

    /**
     * @brief Disable ray-tracing (FUTURE STUB)
     */
    void DisableSionnaRayTracing();

    /**
     * @brief Check if ray-tracing is enabled (FUTURE STUB)
     */
    bool IsRayTracingEnabled() const;

  protected:
    void DoDispose() override;

  private:
    Ptr<NrSimConfig> m_config;           //!< Simulation configuration
    Ptr<NrChannelHelper> m_channelHelper; //!< 5G-LENA channel helper
    bool m_configured;                   //!< Configuration status

    // Channel parameters
    double m_frequency;              //!< Carrier frequency (Hz)
    double m_bandwidth;              //!< Bandwidth (Hz)
    std::string m_propagationModel;  //!< Propagation model name

    // Sionna-RT state (FUTURE)
    bool m_rayTracingEnabled;        //!< Ray-tracing enabled flag
    std::string m_sionnaServerUrl;   //!< Sionna-RT server URL
    std::string m_sceneFile;         //!< 3D scene file path
    
    /**
     * @brief Get 3GPP scenario string for ConfigureFactories
     * @return Scenario string (e.g., "UMa", "UMi-StreetCanyon")
     */
    std::string GetScenarioString() const;

    /**
     * @brief Print detailed channel characteristics
     */
    void PrintChannelDetails() const;
};

} // namespace ns3

#endif // NR_CHANNEL_MANAGER_H