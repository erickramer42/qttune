#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "qttune/core.h"
}

TEST(CoreTest, StatusStringValidValues) {
    // Test all status codes have human-readable strings
    EXPECT_STREQ(qttune_status_string(QT_OK), "success");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NULL_ARGUMENT), "null argument");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NOT_IMPLEMENTED), "not implemented yet");
    EXPECT_STREQ(qttune_status_string(QT_ERR_NO_TRANSPORT), "no transport available");
    EXPECT_STREQ(qttune_status_string(QT_ERR_OUT_OF_MEMORY), "out of memory");
    EXPECT_STREQ(qttune_status_string(QT_ERR_SESSION_CLOSED), "session already closed");
    
    // Test unknown status
    EXPECT_STRNE(qttune_status_string(999), nullptr);
}

TEST(CoreTest, VersionStringContainsNumbers) {
    const char* version = qttune_version_string();
    EXPECT_TRUE(version != nullptr);
    EXPECT_GT(strlen(version), 0);
    
    // Should look like "0.1.0" or similar semver
    EXPECT_NE(strstr(version, "."), nullptr);
}

TEST(CoreTest, InitShutdownIdempotent) {
    // Multiple init calls should not fail
    EXPECT_EQ(qttune_init(), QT_OK);
    EXPECT_EQ(qttune_init(), QT_OK);
    
    // Shutdown once
    qttune_shutdown();
    
    // Can re-init after shutdown
    EXPECT_EQ(qttune_init(), QT_OK);
    qttune_shutdown();
}

TEST(CoreTest, SessionCreateRequiresNonNullPointer) {
    qttune_session* session = nullptr;
    EXPECT_EQ(qttune_session_create("mock://test", &session), QT_ERR_NOT_IMPLEMENTED);
    
    // Null pointer should fail
    EXPECT_EQ(qttune_session_create("mock://test", nullptr), QT_ERR_NULL_ARGUMENT);
}
