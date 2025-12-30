cmake_minimum_required(VERSION 3.15)
project(ore_xccy_curve VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Get paths from environment variables
if(DEFINED ENV{ORE_ROOT} AND NOT ORE_ROOT)
    set(ORE_ROOT "$ENV{ORE_ROOT}" CACHE PATH "Path to ORE installation")
endif()
if(DEFINED ENV{QUANTLIB_ROOT} AND NOT QUANTLIB_ROOT)
    set(QUANTLIB_ROOT "$ENV{QUANTLIB_ROOT}" CACHE PATH "Path to QuantLib installation")
endif()
if(DEFINED ENV{BOOST_ROOT} AND NOT BOOST_ROOT)
    set(BOOST_ROOT "$ENV{BOOST_ROOT}" CACHE PATH "Path to Boost installation")
endif()

# Find Boost (required by QuantLib and ORE)
find_package(Boost REQUIRED)

# Try to find QuantLib via CMake config first
find_package(QuantLib QUIET)

if(NOT QuantLib_FOUND)
    message(STATUS "QuantLib CMake config not found, searching manually...")

    # Search paths for QuantLib
    set(QUANTLIB_SEARCH_PATHS
        ${QUANTLIB_ROOT}
        ${ORE_ROOT}
        "C:/QuantLib"
        "C:/local/QuantLib"
        "/usr/local"
        "/opt/QuantLib"
    )

    # Find QuantLib include directory
    find_path(QUANTLIB_INCLUDE_DIR
        NAMES ql/quantlib.hpp
        PATHS ${QUANTLIB_SEARCH_PATHS}
        PATH_SUFFIXES include
    )

    # Find QuantLib library
    find_library(QUANTLIB_LIBRARY
        NAMES QuantLib QuantLib-x64-mt QuantLib-vc143-x64-mt
        PATHS ${QUANTLIB_SEARCH_PATHS}
        PATH_SUFFIXES lib lib64
    )

    if(QUANTLIB_INCLUDE_DIR AND QUANTLIB_LIBRARY)
        message(STATUS "Found QuantLib: ${QUANTLIB_LIBRARY}")
        message(STATUS "QuantLib include: ${QUANTLIB_INCLUDE_DIR}")
        set(QUANTLIB_FOUND TRUE)
    else()
        message(FATAL_ERROR "QuantLib not found. Set QUANTLIB_ROOT or ORE_ROOT environment variable.")
    endif()
endif()

# Find ORE libraries
message(STATUS "Searching for ORE libraries...")

set(ORE_SEARCH_PATHS
    ${ORE_ROOT}
    "C:/ORE"
    "C:/local/ORE"
    "/usr/local"
    "/opt/ORE"
)

# Find ORE include directory
find_path(ORE_INCLUDE_DIR
    NAMES qle/version.hpp ored/version.hpp
    PATHS ${ORE_SEARCH_PATHS}
    PATH_SUFFIXES include
)

# Find ORE libraries
find_library(QUANTEXT_LIBRARY
    NAMES QuantExt QuantExt-x64-mt
    PATHS ${ORE_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
)
find_library(OREDATA_LIBRARY
    NAMES OREData OREData-x64-mt
    PATHS ${ORE_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
)
find_library(OREANALYTICS_LIBRARY
    NAMES OREAnalytics OREAnalytics-x64-mt
    PATHS ${ORE_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
)

if(ORE_INCLUDE_DIR AND QUANTEXT_LIBRARY)
    message(STATUS "Found ORE: ${ORE_INCLUDE_DIR}")
    message(STATUS "  QuantExt: ${QUANTEXT_LIBRARY}")
    message(STATUS "  OREData: ${OREDATA_LIBRARY}")
    message(STATUS "  OREAnalytics: ${OREANALYTICS_LIBRARY}")
    set(ORE_FOUND TRUE)
    set(ORE_INCLUDE_DIRS ${ORE_INCLUDE_DIR})
    set(ORE_LIBRARIES ${QUANTEXT_LIBRARY} ${OREDATA_LIBRARY} ${OREANALYTICS_LIBRARY})
else()
    message(FATAL_ERROR "ORE not found. Set ORE_ROOT environment variable.")
endif()

# Library sources
set(LIB_SOURCES
    src/market_data.cpp
    src/curve_builder.cpp
    src/curve_utils.cpp
)

set(LIB_HEADERS
    include/ore_xccy_curve/market_data.hpp
    include/ore_xccy_curve/curve_builder.hpp
    include/ore_xccy_curve/curve_utils.hpp
)

# Create library
add_library(ore_xccy_curve ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(ore_xccy_curve
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${ORE_INCLUDE_DIRS}
        ${QUANTLIB_INCLUDE_DIR}
)

# Link libraries - use CMake target if available, otherwise use found library
if(TARGET QuantLib::QuantLib)
    target_link_libraries(ore_xccy_curve
        PUBLIC
            Boost::boost
            QuantLib::QuantLib
            ${ORE_LIBRARIES}
    )
else()
    target_link_libraries(ore_xccy_curve
        PUBLIC
            Boost::boost
            ${QUANTLIB_LIBRARY}
            ${ORE_LIBRARIES}
    )
endif()

# Example executable
add_executable(xccy_curve_example examples/main.cpp)
target_link_libraries(xccy_curve_example PRIVATE ore_xccy_curve)

# Python bindings (optional)
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" ON)

if(BUILD_PYTHON_BINDINGS)
    find_package(pybind11 CONFIG QUIET)

    if(NOT pybind11_FOUND)
        # Try to fetch pybind11 if not found
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(_core ore_xccy_curve_cpp/bindings.cpp)
    target_link_libraries(_core PRIVATE ore_xccy_curve)

    # Install Python module to package directory
    install(TARGETS _core
        LIBRARY DESTINATION ore_xccy_curve_cpp
        COMPONENT python
    )
endif()

# Installation
install(TARGETS ore_xccy_curve
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
