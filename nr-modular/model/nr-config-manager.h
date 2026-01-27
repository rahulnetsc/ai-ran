/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef NR_CONFIG_MANAGER_H
#define NR_CONFIG_MANAGER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "utils/nr-sim-config.h"

namespace ns3
{

/**
 * @brief Manager for loading, validating, and saving NR simulation configurations
 * 
 * Now supports JSON format exclusively. Provides:
 * - Configuration loading from JSON files
 * - Validation with detailed error messages
 * - Configuration saving to JSON
 * - Default configuration creation
 */
class NrConfigManager : public Object
{
  public:
    static TypeId GetTypeId();
    
    NrConfigManager();
    ~NrConfigManager() override;

    // ========================================================================
    // PRIMARY METHODS
    // ========================================================================

    /**
     * @brief Load configuration from JSON file
     * @param filePath Path to JSON configuration file
     * @return Ptr to loaded NrSimConfig object
     * 
     * This method:
     * 1. Checks if file exists
     * 2. Creates NrSimConfig object
     * 3. Calls config->LoadFromJson()
     * 4. Returns loaded config
     */
    Ptr<NrSimConfig> LoadFromFile(const std::string& filePath);

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     * 
     * Performs both:
     * - Built-in NrSimConfig::Validate()
     * - Additional file existence checks
     */
    bool Validate(const Ptr<NrSimConfig>& config) const;

    /**
     * @brief Validate configuration or abort simulation
     * @param config Configuration to validate
     * 
     * Convenience method that calls Validate() and aborts if invalid
     */
    void ValidateOrAbort(const Ptr<NrSimConfig>& config) const;

    // ========================================================================
    // UTILITY METHODS
    // ========================================================================

    /**
     * @brief Save configuration to JSON file
     * @param config Configuration to save
     * @param filePath Output file path
     */
    void SaveToJson(const Ptr<NrSimConfig>& config, const std::string& filePath) const;

    /**
     * @brief Create default configuration
     * @return Ptr to new NrSimConfig with default values
     */
    Ptr<NrSimConfig> CreateDefaultConfig() const;

    /**
     * @brief Print configuration summary
     * @param config Configuration to print
     * @param os Output stream
     */
    void PrintConfigSummary(const Ptr<NrSimConfig>& config, std::ostream& os) const;

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Check if file exists
     * @param filePath File path to check
     * @return true if file exists and is readable
     */
    bool FileExists(const std::string& filePath) const;
};

} // namespace ns3

#endif // NR_CONFIG_MANAGER_H