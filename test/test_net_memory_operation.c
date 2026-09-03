#include "unity.h"
#include "network/net_memory_operation.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_processRequestVersion(void) {
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    RequestVersionOperation socket_op = {
        .header = {
            .type = 0,
            .keep_alive = 1
        }
    };
    
    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    int i = 0;
    u32 api = get32FromBuffer(output, &i);
    u32 max_input = get32FromBuffer(output, &i);
    u32 max_output = get32FromBuffer(output, &i);
    u32 max_addresses = get32FromBuffer(output, &i);
    u32 major_version = get32FromBuffer(output, &i);
    u32 minor_version = get32FromBuffer(output, &i);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(API_VERSION, api, "Field 0-3 should be API_VERSION");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MAX_INPUT_BYTES, max_input, "Field 4-7 should be MAX_INPUT_BYTES");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MAX_OUTPUT_BYTES, max_output, "Field 8-11 should be MAX_OUTPUT_BYTES");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MAX_ABSOLUTE_ADDRESSES, max_addresses, "Field 12-15 should be MAX_ABSOLUTE_ADDRESSES");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(NIN_MAJOR_VERSION, major_version, "Field 16-19 should be NIN_MAJOR_VERSION");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(NIN_MINOR_VERSION, minor_version, "Field 20-23 should be NIN_MINOR_VERSION");
    TEST_ASSERT_EQUAL_INT_MESSAGE(24, result, "Should only return 7 U32s.");
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_processRequestVersion);
    return UNITY_END();
}