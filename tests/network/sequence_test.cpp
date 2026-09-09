#include "lcu/network/sequence.h"

#include <gtest/gtest.h>

using lcu::network::sequence_greater_than;

TEST(Sequence, SimpleGreaterThan) {
    EXPECT_TRUE(sequence_greater_than(5, 3));
    EXPECT_FALSE(sequence_greater_than(3, 5));
    EXPECT_FALSE(sequence_greater_than(5, 5));
}

TEST(Sequence, WrapAroundIsHandledCorrectly) {
    // 0 comes right after 65535 - it's "greater" (newer) despite the
    // smaller numeric value.
    EXPECT_TRUE(sequence_greater_than(0, 65535));
    EXPECT_FALSE(sequence_greater_than(65535, 0));
}

TEST(Sequence, SmallWrapAroundWindow) {
    EXPECT_TRUE(sequence_greater_than(2, 65534));
    EXPECT_FALSE(sequence_greater_than(65534, 2));
}

TEST(Sequence, FarApartWithinHalfRangeIsOrdinaryComparison) {
    EXPECT_TRUE(sequence_greater_than(1000, 10));
    EXPECT_FALSE(sequence_greater_than(10, 1000));
}
