# Copyright (c) 2020 vesoft inc. All rights reserved.
#
# This source code is licensed under Apache 2.0 License.

nebula_add_library(
    meta_obj OBJECT
    GflagsManager.cpp
    SchemaProviderIf.cpp
    NebulaSchemaProvider.cpp
    SchemaManager.cpp
    ServerBasedSchemaManager.cpp
    IndexManager.cpp
    ServerBasedIndexManager.cpp
)
