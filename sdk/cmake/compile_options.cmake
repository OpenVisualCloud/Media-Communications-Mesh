# SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# Default compiler and linker definitions and flags

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -static")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -static -s -Werror -Wall")

# Compiler flags
# add_compile_options(-fPIE -fPIC)

# Compile definitions
# if(ENABLE_GLOG)
#     add_definitions(-DENABLE_GLOG)
# endif()

# pkg_check_modules(LIBBSD libbsd)
# include_directories(${LIBBSD_INCLUDE_DIRS})
# link_directories(${LIBBSD_LIBRARY_DIRS})

find_library(LIB_BSD bsd)
if(LIB_BSD)
  link_directories(${LIB_BSD})
  # target_link_libraries(memif ${LIB_BSD})
endif()

# link options
set(CMAKE_SKIP_BUILD_RPATH  TRUE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z now -pie")
