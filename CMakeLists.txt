# Copyright 2021 u-blox
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#  http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13.1)

set(DTC_OVERLAY_FILE  "c209.overlay")

set(CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf)
if(DEFINED RELEASE)
  message("RELEASE BUILD")
  add_definitions("-DRELEASE=${RELEASE}")
  list(APPEND CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj_release.conf)
else()
  message("DEBUG BUILD")
  add_definitions("-DDEBUG=${DEBUG}")
  list(APPEND CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj_debug.conf)
endif()

if(DEFINED NRF_DFU_BOOT_SUPPORT)
message("NRF_DFU_BOOT_SUPPORT BUILD")
add_definitions("-DNRF_DFU_BOOT_SUPPORT=${NRF_DFU_BOOT_SUPPORT}")
list(APPEND CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj_dfu_boot.conf)
endif()

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(direction_finding_beacon)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_sources(app PRIVATE ubx_version.c)

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)

# Get git commit and  build time and pass it to version_config.h

execute_process(COMMAND
  git describe --always --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

string(TIMESTAMP BUILD_TIME "%Y-%d-%m %H:%M")

message("GIT commit: ${GIT_SHA1}")
message("Build time: ${BUILD_TIME}")

add_definitions("-DGIT_SHA1=${GIT_SHA1}")
add_definitions("-DBUILD_TIME=${BUILD_TIME}")

configure_file(version_config.h.in ${CMAKE_BINARY_DIR}/generated/version_config.h)
include_directories( ${CMAKE_BINARY_DIR}/generated/)

target_compile_options(app PRIVATE -Wall -Werror -Wno-stringop-truncationw)
