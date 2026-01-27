/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_TOPOLOGY_MANAGER_H
#define NR_TOPOLOGY_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node-container.h"
#include "ns3/vector.h"

#include <vector>

namespace ns3
{

class NrSimConfig;

/**
 * @brief Manager for network topology creation and node deployment
 * 
 * Handles:
 * - gNB deployment (positions)
 * - UE deployment (positions)
 * - Node creation
 * - Topology validation
 * - Smart mobility model selection (Waypoint vs RandomWalk vs Static)
 */
class NrTopologyManager : public Object
{
  public:
    static TypeId GetTypeId();

    NrTopologyManager();
    ~NrTopologyManager() override;

    /**
     * @brief Set simulation configuration
     * @param config Pointer to NrSimConfig object
     */
    void SetConfig(const Ptr<NrSimConfig>& config);

    /**
     * @brief Deploy network topology
     * 
     * Creates nodes and positions them according to configuration:
     * - Creates gNB nodes
     * - Creates UE nodes
     * - Installs appropriate mobility models (Waypoint/RandomWalk/Static)
     * - Sets positions for gNBs and UEs
     */
    void DeployTopology();

    /**
     * @brief Get gNB node container
     * @return Container of gNB nodes
     */
    NodeContainer GetGnbNodes() const;

    /**
     * @brief Get UE node container
     * @return Container of UE nodes
     */
    NodeContainer GetUeNodes() const;

    /**
     * @brief Check if topology has been deployed
     * @return True if DeployTopology() has been called
     */
    bool IsDeployed() const;

    /**
     * @brief Get number of gNBs
     * @return Number of gNB nodes
     */
    uint32_t GetNumGnbs() const;

    /**
     * @brief Get number of UEs
     * @return Number of UE nodes
     */
    uint32_t GetNumUes() const;

    /**
     * @brief Get gNB positions
     * @return Vector of gNB positions
     */
    std::vector<Vector> GetGnbPositions() const;

    /**
     * @brief Get UE positions
     * @return Vector of UE positions
     */
    std::vector<Vector> GetUePositions() const;

  protected:
    void DoDispose() override;

  private:
    Ptr<NrSimConfig> m_config;    //!< Simulation configuration
    bool m_deployed;              //!< Deployment status flag

    NodeContainer m_gnbNodes;     //!< gNB nodes
    NodeContainer m_ueNodes;      //!< UE nodes

    std::vector<Vector> m_gnbPositions;  //!< gNB positions
    std::vector<Vector> m_uePositions;   //!< UE positions

    /**
     * @brief Deploy nodes from position file
     * 
     * Reads gNB and UE positions from configured file
     */
    void DeployFromFile();

    /**
     * @brief Deploy nodes in hexagonal pattern
     * 
     * Places gNBs at center and UEs based on placement strategy:
     * - uniform/random: Random distribution
     * - grid: Grid layout
     * - circle (default): Circle around gNB
     */
    void DeployHexagonal();

    /**
     * @brief Set initial positions for waypoint UEs
     * 
     * For UEs with configured waypoints, override their initial
     * position to be the first waypoint in their path
     */
    void SetInitialPositionsFromWaypoints();
};

} // namespace ns3

#endif // NR_TOPOLOGY_MANAGER_H