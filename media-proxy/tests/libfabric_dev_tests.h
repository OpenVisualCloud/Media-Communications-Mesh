#ifndef LIBFABRIC_DEV_TESTS_H
#define LIBFABRIC_DEV_TESTS_H

#include <fff.h>
#include <gtest/gtest.h>
#include "libfabric_dev.h"
#include "libfabric_mocks.h"  // This header declares fi_getinfo_custom_fake and custom_close.

class LibfabricDevTest : public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    // Test context used by libfabric_dev.
    libfabric_ctx* ctx;
};

#endif // LIBFABRIC_DEV_TESTS_H