#include "ns3/test.h"
#include "utils/nr-sim-config.h"

using namespace ns3;

class NrSimConfigTestCase : public TestCase
{
public:
    NrSimConfigTestCase() : TestCase("NrSimConfig basic test") {}
    
    void DoRun() override
    {
        // Test 1: Can we create it?
        Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
        NS_TEST_ASSERT_MSG_NE(config, nullptr, "Failed to create NrSimConfig");
        
        // Test 2: Are defaults correct?
        NS_TEST_ASSERT_MSG_EQ(config->topology.gnbCount, 1, "Wrong default gnbCount");
        NS_TEST_ASSERT_MSG_EQ(config->topology.ueCount, 10, "Wrong default ueCount");
        
        // Test 3: Can we modify values?
        config->topology.gnbCount = 3;
        NS_TEST_ASSERT_MSG_EQ(config->topology.gnbCount, 3, "Failed to set gnbCount");
        
        // Test 4: Does validation work?
        NS_TEST_ASSERT_MSG_EQ(config->Validate(), true, "Valid config failed validation");
        
        // Test 5: Does validation catch errors?
        config->topology.gnbCount = 0;
        NS_TEST_ASSERT_MSG_EQ(config->Validate(), false, "Invalid config passed validation");
        
        // Test 6: Can we print?
        std::ostringstream oss;
        config->Print(oss);
        NS_TEST_ASSERT_MSG_NE(oss.str().length(), 0, "Print produced no output");
    }
};

static NrSimConfigTestCase g_nrSimConfigTestCase;