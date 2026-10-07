#include <gtest/gtest.h>
#include <qttune/signals.h>
#include <qttune/signal_sets.h>
#include <cstring>
#include <cmath>

class SignalDecodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SignalDecodeTest, ValidUint8Decode) {
    QttuneSignalDef signal = {};
    signal.id = 1001;
    strncpy(signal.name, "Engine_RPM", sizeof(signal.name));
    signal.type = QTTUNE_SIGNAL_TYPE_UINT16;
    signal.start_byte = 0;
    signal.num_bytes = 2;
    signal.bit_offset = 0;
    signal.bit_length = 16;
    signal.scale = 0.1f;
    signal.offset = 0.0f;
    strncpy(signal.unit, "RPM", sizeof(signal.unit));
    signal.decimal_places = 1;
    
    uint8_t frame[8] = {0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* 1000 RPM */
    float value = 0.0f;
    
    int result = qttune_decode_signal(&signal, frame, 8, &value);
    
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(value, 100.0f, 0.01f);
}

TEST_F(SignalDecodeTest, ValidInt8WithSignExtension) {
    QttuneSignalDef signal = {};
    signal.id = 2001;
    strncpy(signal.name, "Coolant_Temp", sizeof(signal.name));
    signal.type = QTTUNE_SIGNAL_TYPE_INT8;
    signal.start_byte = 1;
    signal.num_bytes = 1;
    signal.bit_offset = 0;
    signal.bit_length = 8;
    signal.scale = 1.0f;
    signal.offset = -40.0f;
    strncpy(signal.unit, "°C", sizeof(signal.unit));
    signal.decimal_places = 0;
    
    uint8_t frame[8] = {0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* -1 °C after offset */
    float value = 0.0f;
    
    int result = qttune_decode_signal(&signal, frame, 8, &value);
    
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(value, -41.0f, 0.01f); /* -1 + (-40) = -41 */
}

TEST_F(SignalDecodeTest, NullPointerReturnsError) {
    QttuneSignalDef signal = {};
    uint8_t frame[8] = {0x00};
    float value = 0.0f;
    
    EXPECT_EQ(qttune_decode_signal(nullptr, frame, 8, &value), 1);
    EXPECT_EQ(qttune_decode_signal(&signal, nullptr, 8, &value), 1);
    EXPECT_EQ(qttune_decode_signal(&signal, frame, 8, nullptr), 1);
}

TEST_F(SignalDecodeTest, BoundsViolationReturnsError) {
    QttuneSignalDef signal = {};
    signal.start_byte = 10; /* Beyond frame length */
    signal.num_bytes = 2;
    signal.bit_offset = 0;
    signal.bit_length = 16;
    
    uint8_t frame[8] = {0x00};
    float value = 0.0f;
    
    EXPECT_EQ(qttune_decode_signal(&signal, frame, 8, &value), 2);
}

TEST_F(SignalDecodeTest, InvalidBitLengthReturnsError) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_UINT8;
    signal.bit_length = 0; /* Invalid */
    
    uint8_t frame[8] = {0x00};
    float value = 0.0f;
    
    EXPECT_EQ(qttune_decode_signal(&signal, frame, 8, &value), 3);
}

TEST_F(SignalDecodeTest, UnsupportedTypeReturnsError) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_STRING; /* Not implemented yet */
    
    uint8_t frame[8] = {0x00};
    float value = 0.0f;
    
    EXPECT_EQ(qttune_decode_signal(&signal, frame, 8, &value), 4);
}

TEST_F(SignalDecodeTest, ValidateSignal_Success) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_UINT16;
    signal.num_bytes = 2;
    signal.bit_length = 16;
    signal.bit_offset = 0;
    strncpy(signal.name, "Test_Signal", sizeof(signal.name));
    strncpy(signal.unit, "units", sizeof(signal.unit));
    signal.decimal_places = 2;
    
    EXPECT_EQ(qttune_validate_signal(&signal), 0);
}

TEST_F(SignalDecodeTest, ValidateSignal_InvalidNumBytes) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_UINT16;
    signal.num_bytes = 0; /* Invalid */
    
    EXPECT_EQ(qttune_validate_signal(&signal), 2);
}

TEST_F(SignalDecodeTest, ValidateSignal_UnterminatedName) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_UINT8;
    signal.num_bytes = 1;
    signal.bit_length = 8;
    /* Fill ALL 32 bytes with non-null chars — no room for terminator */
    /* Intentional: tests validator's handling of unterminated strings */
    /* Code scanning may flag, do not fix */
    memset(signal.name, 'A', sizeof(signal.name));
    
    EXPECT_EQ(qttune_validate_signal(&signal), 6);
}

TEST_F(SignalDecodeTest, ScaleOffsetCalculation) {
    QttuneSignalDef signal = {};
    signal.type = QTTUNE_SIGNAL_TYPE_UINT16;
    signal.start_byte = 0;
    signal.num_bytes = 2;
    signal.bit_offset = 0;
    signal.bit_length = 16;
    signal.scale = 0.01f;
    signal.offset = 100.0f;
    
    uint8_t frame[8] = {0xD0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* 2000 raw */
    float value = 0.0f;
    
    int result = qttune_decode_signal(&signal, frame, 8, &value);
    
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(value, 120.0f, 0.01f); /* 2000 * 0.01 + 100 = 120 */
}

TEST(SignalsTest, MockSignalSetPassesValidation) {
    EXPECT_EQ(qttune_validate_signal_set(qttune_mock_signal_set()), 0);
}

TEST(SignalsTest, MockSetIdsAreUnique) {
    const QttuneSignalSet* s = qttune_mock_signal_set();
    for (uint32_t i = 0; i < s->count; ++i)
        for (uint32_t j = i + 1; j < s->count; ++j)
            EXPECT_NE(s->defs[i].id, s->defs[j].id)
                << "duplicate ids at " << i << "," << j;
}

TEST(SignalsTest, MockSetDecodesReasonably) {
    /* Integration test: verify mock set definitions decode to plausible ranges */
    const QttuneSignalSet* set = qttune_mock_signal_set();
    uint8_t frame[8] = {0xE8, 0x03,  // RPM raw 1000
                        0x64,       // Coolant raw 100 → 60°C
                        0x3C,       // Speed raw 60 → 60 km/h
                        0x80,       // Throttle raw 128 → 50.2%
                        0x74,       // IAT raw 116 → 76°C
                        0xCD,       // Load raw 0xCD → 50.2%
                        0x00};      // padding

    for (uint32_t i = 0; i < set->count; ++i) {
        float val = 0.0f;
        int rc = qttune_decode_signal(&set->defs[i], frame, 8, &val);
        EXPECT_EQ(rc, 0) << "decode failed for " << set->defs[i].name << " at index " << i;

        /* Plausibility checks per signal */
        switch (set->defs[i].id) {
        case MOCK_SIG_RPM_ID:
            EXPECT_GE(val, 0.0f);
            EXPECT_LE(val, 8000.0f);
            break;
        case MOCK_SIG_COOLANT_ID:
        case MOCK_SIG_IAT_ID:
            EXPECT_GE(val, -40.0f);
            EXPECT_LE(val, 215.0f);
            break;
        case MOCK_SIG_SPEED_ID:
            EXPECT_GE(val, 0.0f);
            EXPECT_LE(val, 255.0f);
            break;
        case MOCK_SIG_THROTTLE_ID:
        case MOCK_SIG_LOAD_ID:
            EXPECT_GE(val, 0.0f);
            EXPECT_LE(val, 100.0f);
            break;
        }
    }
}
