cmake_minimum_required(VERSION 3.18)
project(gwm VERSION 3.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT GWM_IPO_SUPPORTED OUTPUT _ipo_msg)
if(GWM_IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# The CLI (make/plain cmake) build is unaffected unless this is turned on.
option(GWM_BUILD_PYTHON "Build the gwm._core pybind11 extension module" OFF)

add_library(gwm_core STATIC
  src/Graph.cpp
  src/WLKernel.cpp
  src/gWM.cpp
  src/rank9sel.cpp
)
target_include_directories(gwm_core PUBLIC src)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(gwm_core PRIVATE -Wall -Wextra)
  # rank9sel is vendored third-party code (sux); keep its warnings quiet.
  set_source_files_properties(src/rank9sel.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-private-field")
endif()

add_executable(gwm-build src/Build.cpp)
target_link_libraries(gwm-build PRIVATE gwm_core)

add_executable(gwm-search src/Search.cpp)
target_link_libraries(gwm-search PRIVATE gwm_core)

# SKBUILD is set by scikit-build-core: the wheel ships only the _core
# extension module (below), not these CLI binaries.
if(NOT SKBUILD)
  install(TARGETS gwm-build gwm-search RUNTIME DESTINATION bin)
endif()

if(GWM_BUILD_PYTHON)
  # gwm_core is a static library; it must be position-independent to be
  # linked into the _core.so extension module.
  set_target_properties(gwm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

  find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)

  pybind11_add_module(_core python/bindings.cpp)
  target_link_libraries(_core PRIVATE gwm_core)
  target_compile_definitions(_core PRIVATE GWM_VERSION="${PROJECT_VERSION}")

  install(TARGETS _core LIBRARY DESTINATION gwm)
endif()
