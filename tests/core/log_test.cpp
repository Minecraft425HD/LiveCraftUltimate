#include "lcu/core/log.h"

#include <gtest/gtest.h>

TEST(Log, LevelTagsAreStable) {
    EXPECT_STREQ(lcu::log::level_tag(lcu::log::Level::Trace), "TRACE");
    EXPECT_STREQ(lcu::log::level_tag(lcu::log::Level::Debug), "DEBUG");
    EXPECT_STREQ(lcu::log::level_tag(lcu::log::Level::Info), "INFO");
    EXPECT_STREQ(lcu::log::level_tag(lcu::log::Level::Warn), "WARN");
    EXPECT_STREQ(lcu::log::level_tag(lcu::log::Level::Error), "ERROR");
}

TEST(Log, MacrosDoNotCrash) {
    LCU_LOG_TRACE("trace {}", 1);
    LCU_LOG_DEBUG("debug {}", 2);
    LCU_LOG_INFO("info {}", 3);
    LCU_LOG_WARN("warn {}", 4);
    LCU_LOG_ERROR("error {}", 5);
    SUCCEED();
}
