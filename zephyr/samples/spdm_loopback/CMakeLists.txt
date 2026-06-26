# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 DMTF. All rights reserved.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spdm_loopback)

target_sources(app PRIVATE
  src/main.c
  src/mock_transport.c
  src/requester.c
  src/responder.c
  ${CMAKE_CURRENT_LIST_DIR}/../common/sample_ecp256.c
  ${CMAKE_CURRENT_LIST_DIR}/../common/sample_app_message.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../common)
