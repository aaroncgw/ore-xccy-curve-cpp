cmake_minimum_required(VERSION 3.15)
project(ore_xccy_curve VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Boost (required by QuantLib and ORE)
find_package(Boost REQUIRED)

# Find QuantLib and ORE
# Adjust these paths based on your installation
find_package(QuantLib REQUIRED)
find_package(ORE QUIET)

# If ORE is not found via find_package, try to locate it manually
if(NOT ORE_FOUND)
    # Common installation paths
    set(ORE_ROOT "" CACHE PATH "Path to ORE installation")

    if(ORE_ROOT)
        set(ORE_INCLUDE_DIRS "${ORE_ROOT}/include")
        set(ORE_LIBRARY_DIRS "${ORE_ROOT}/lib")

        find_library(QUANTEXT_LIBRARY
            NAMES QuantExt
            PATHS ${ORE_LIBRARY_DIRS}
        )
        find_library(OREDATA_LIBRARY
            NAMES OREData
            PATHS ${ORE_LIBRARY_DIRS}
        )
        find_library(OREANALYTICS_LIBRARY
            NAMES OREAnalytics
            PATHS ${ORE_LIBRARY_DIRS}
        )

        set(ORE_LIBRARIES ${QUANTEXT_LIBRARY} ${OREDATA_LIBRARY} ${OREANALYTICS_LIBRARY})
    endif()
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
)

target_link_libraries(ore_xccy_curve
    PUBLIC
        Boost::boost
        QuantLib::QuantLib
        ${ORE_LIBRARIES}
)

# Example executable
add_executable(xccy_curve_example examples/main.cpp)
target_link_libraries(xccy_curve_example PRIVATE ore_xccy_curve)

# Installation
install(TARGETS ore_xccy_curve
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
