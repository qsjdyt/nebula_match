# Copyright (c) 2020 vesoft inc. All rights reserved.
#
# This source code is licensed under Apache 2.0 License.

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Version.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
  )

nebula_add_library(
  version_obj
  OBJECT
  ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
  )
