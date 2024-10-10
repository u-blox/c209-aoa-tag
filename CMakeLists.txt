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

list(APPEND DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/c209.overlay)

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
list(APPEND DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/c209_nrf_boot.overlay)
endif()

if(DEFINED MCUBOOT_BOOT_SUPPORT)
message("MCUBOOT_BOOT_SUPPORT BUILD")
add_definitions("-DMCUBOOT_BOOT_SUPPORT=${MCUBOOT_BOOT_SUPPORT}")
list(APPEND CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ci/testable.conf)
list(APPEND DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ci/c209_mcuboot.overlay)
endif()

# For applying SDK patches automatically on build.
function(applyGitPatchFiles patchFolder applyDir)
  message("Applying git patches from ${patchFolder} in directory ${applyDir}")
  file(GLOB PATCHES ${patchFolder})
  if (PATCHES)
    message(STATUS "Patches: ${PATCHES}")
    foreach(PATCH ${PATCHES})
      message(STATUS "Applying ${PATCH}")
      execute_process(
        COMMAND git apply -p1 --ignore-whitespace
        WORKING_DIRECTORY "${applyDir}"
        INPUT_FILE "${PATCH}"
        OUTPUT_VARIABLE OUTPUT
        RESULT_VARIABLE RESULT)
      if (RESULT EQUAL 0)
        message(STATUS "Patch applied: ${PATCH}")
      else()
        # Unfortunately although patch will recognise that a patch is already
        # applied it will still return an error.
        execute_process(
          COMMAND git apply -p1 --reverse --check --ignore-whitespace
          WORKING_DIRECTORY "${applyDir}"
          INPUT_FILE "${PATCH}"
          OUTPUT_VARIABLE OUTPUT
          RESULT_VARIABLE RESULT2)
        if (RESULT2 EQUAL 0)
          message(STATUS "Patch was already applied, ignore errors above: ${PATCH}")
        else()
          message(FATAL_ERROR "Error applying patch ${PATCH}")
        endif()
      endif()
    endforeach(PATCH)
  endif()
endfunction()

applyGitPatchFiles("${CMAKE_SOURCE_DIR}/sdk-patches/zephyr/*.patch" "$ENV{ZEPHYR_BASE}/")

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
