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


#pragma region processArrayOperationsTest

void test_processArrayOperation_normal(void) {
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    ReadArrayOperation socket_op = {
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 2,
        .size = 4,
        .stride = 5
    };

    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, result, "Should return 9 bytes (1 success byte + 2 * 4 bytes of data).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");
    // TODO: use mocking and assert for correct addresses being called
}

void test_processArrayOperation_zeros(void) {
    // 0 count
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    ReadArrayOperation socket_op = {
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 0,
        .size = 4,
        .stride = 5
    };

    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result, "Should return 1 byte (success byte).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");

    // 0 Size
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 1,
        .size = 0,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result, "Should return 1 byte (success byte).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");

    // 0 stride
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 1,
        .size = 4,
        .stride = 0
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, result, "Should return 5 byte (success byte + 1*4 bytes).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");
    // TODO: use mocking and assert for correct addresses being called
}

// TODO: seperate test stride smaller than size? need to check on hardware first whether wii likes that...

// weird alignments
void test_processArrayOperation_weird_alignments(void) {
    // Small
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    ReadArrayOperation socket_op = {
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000001, 
        .count = 2,
        .size = 4,
        .stride = 5
    };

    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, result, "Should return 9 bytes (1 success byte + 2 * 4 bytes of data).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");
    // TODO: use mocking and assert for correct addresses being called

    // Big 
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000006, 
        .count = 1,
        .size = 16,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(17, result, "Should return 17 bytes (1 success byte + 1 * 16 bytes of data).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");
    // TODO: use mocking and assert for correct addresses being called
}

void test_processArrayOperation_out_of_range_addresses(void) {
    // Under
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    ReadArrayOperation socket_op = {
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x7FFFFFFF, 
        .count = 1,
        .size = 4,
        .stride = 5
    };

    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Should return 0 bytes (special case).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, output[0], "First byte should indicate failure.");

    // Over
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x82400000, 
        .count = 1,
        .size = 4,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Should return 0 bytes (special case).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, output[0], "First byte should indicate failure.");

    // In between but going over
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x823FFFFF, 
        .count = 1,
        .size = 2,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Should return 0 bytes (special case).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, output[0], "First byte should indicate failure.");
}

void test_processArrayOperation_overflow_outputs(void) {
    // over max output at once
    // Setup
    u8 output[MAX_OUTPUT_BYTES];
    ReadArrayOperation socket_op = {
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 1,
        .size = MAX_OUTPUT_BYTES,
        .stride = 5
    };

    // Run 
    int result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Should return 0 bytes (special case).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, output[0], "First byte should indicate failure.");

    // over max output at second time
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 2,
        .size = MAX_OUTPUT_BYTES/2,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Should return 0 bytes (special case).");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, output[0], "First byte should indicate failure.");

    // just fitting to max output
    // Setup
    socket_op = (ReadArrayOperation){
        .header = {
            .type = 2,
            .keep_alive = 1
        },
        .address = 0x80000000, 
        .count = 1,
        .size = MAX_OUTPUT_BYTES-1,
        .stride = 5
    };

    // Run 
    result = processSocketOperation((SocketOperation *)&socket_op, output);

    // Assert
    TEST_ASSERT_EQUAL_INT_MESSAGE(MAX_OUTPUT_BYTES, result, "Should return MAX_OUTPUT_BYTES bytes.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, output[0], "First byte should indicate success.");
}

#pragma endregion

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_processRequestVersion);
    RUN_TEST(test_processArrayOperation_normal);
    RUN_TEST(test_processArrayOperation_zeros);
    RUN_TEST(test_processArrayOperation_weird_alignments);
    RUN_TEST(test_processArrayOperation_out_of_range_addresses);
    RUN_TEST(test_processArrayOperation_overflow_outputs);
    return UNITY_END();
}