/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_MOBILITY_MANAGER_H
#define NR_MOBILITY_MANAGER_H

#include "ns3/object.h"
#include "ns3/node-container.h"
#include "ns3/mobility-module.h"

#include "utils/nr-sim-config.h"

namespace ns3
{

/**
 * @brief Manager for installing mobility models on network nodes
 * 
 * Features:
 * - Static mobility for gNBs (ConstantPositionMobilityModel)
 * - Per-UE mobility selection based on configuration:
 *   1. Waypoint mobility (if waypoints defined in config)
 *   2. Default mobility (RandomWalk, ConstantVelocity, etc.)
 * 
 * Waypoint Support:
 * - Reads waypoints from NrSimConfig (loaded from JSON)
 * - Calculates travel times based on speed and distance
 * - Automatically installs WaypointMobilityModel for UEs with paths
 */
class NrMobilityManager : public Object
{
  public:
    static TypeId GetTypeId();

    NrMobilityManager();
    ~NrMobilityManager() override;

    /**
     * @brief Set configuration
     * @param config Simulation configuration with mobility parameters
     */
    void SetConfig(const Ptr<NrSimConfig>& config);

    /**
     * @brief Install mobility on gNB nodes
     * @param gnbNodes Container of gNB nodes
     * 
     * Installs ConstantPositionMobilityModel (gNBs don't move)
     */
    void InstallGnbMobility(const NodeContainer& gnbNodes);

    /**
     * @brief Install mobility on UE nodes
     * @param ueNodes Container of UE nodes
     * 
     * For each UE:
     * 1. Check if waypoints are defined (config->HasUeWaypoints(ueId))
     * 2. If yes: Install WaypointMobilityModel with waypoints
     * 3. If no: Install default mobility model (RandomWalk, etc.)
     */
    void InstallUeMobility(const NodeContainer& ueNodes);

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Install static mobility (no movement)
     * @param node Node to install on
     * @param position Initial position (not used, kept for compatibility)
     */
    void InstallStaticMobility(Ptr<Node> node, const Vector& position);

    /**
     * @brief Install random walk mobility
     * @param node Node to install on
     * 
     * Uses RandomWalk2dMobilityModel with default bounds
     */
    void InstallRandomWalkMobility(Ptr<Node> node);

    /**
     * @brief Install waypoint mobility from config
     * @param node Node to install on
     * @param ueId UE identifier (to lookup waypoints)
     * 
     * Reads waypoints from config->GetUeWaypoints(ueId)
     * Calculates travel times based on speed
     * Installs WaypointMobilityModel
     */
    void InstallWaypointMobilityFromConfig(Ptr<Node> node, uint32_t ueId);

    /**
     * @brief Build NS-3 waypoints from config waypoints
     * @param positions Vector of waypoint positions
     * @param speedMps Speed in meters per second
     * @return Vector of NS-3 Waypoint objects with calculated times
     * 
     * Calculates arrival time at each waypoint based on:
     * - Distance between consecutive waypoints
     * - Specified speed
     */
    std::vector<Waypoint> BuildWaypointsWithTiming(const std::vector<Vector>& positions,
                                                     double speedMps);

    Ptr<NrSimConfig> m_config;  //!< Configuration object
    bool m_installed;            //!< Installation status flag
};

} // namespace ns3

#endif // NR_MOBILITY_MANAGER_H