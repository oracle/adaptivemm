/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * adaptived googletest main entry point
 *
 * Copyright (c) 2023 Oracle and/or its affiliates
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */

#include "gtest/gtest.h"

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
