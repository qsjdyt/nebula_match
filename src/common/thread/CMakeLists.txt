# Copyright (c) 2020 vesoft inc. All rights reserved.
#
# This source code is licensed under Apache 2.0 License.

nebula_add_library(
    thread_obj OBJECT
    NamedThread.cpp
    GenericWorker.cpp
    GenericThreadPool.cpp
)

nebula_add_subdirectory(test)
