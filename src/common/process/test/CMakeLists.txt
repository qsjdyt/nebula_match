# Copyright (c) 2020 vesoft inc. All rights reserved.
#
# This source code is licensed under Apache 2.0 License.

nebula_add_test(
    NAME
        process_test
    SOURCES
        ProcessTest.cpp
    OBJECTS
        $<TARGET_OBJECTS:process_obj>
        $<TARGET_OBJECTS:fs_obj>
        $<TARGET_OBJECTS:base_obj>
    LIBRARIES
        gtest
        gtest_main
)
