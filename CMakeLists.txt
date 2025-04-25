cmake_minimum_required(VERSION 3.14) # Required for FetchContent
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake") 
project(SCC VERSION 0.1.0 LANGUAGES CXX)

# Set C++20 standard (required by YGM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Find MPI package (required by YGM)
find_package(MPI REQUIRED)

#
# Boost
include(setup_boost)
prepare_fetchcontent_boost()
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
        Boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.78.0
        GIT_SUBMODULES ${BOOST_REQD_SUBMODULES}
        GIT_PROGRESS TRUE
        CONFIGURE_COMMAND ""  # tell CMake it's not a cmake project
)
FetchContent_MakeAvailable(Boost)
get_boost_include_dirs()


set(DESIRED_YGM_VERSION 0.8)
find_package(ygm ${DESIRED_YGM_VERSION} CONFIG)
if (NOT ygm_FOUND)
    FetchContent_Declare(
        ygm
        GIT_REPOSITORY /home/westoncadena/llnl/ygm
        GIT_TAG v0.8-dev
    )
    FetchContent_GetProperties(ygm)
    if (ygm_POPULATED)
        message(STATUS "Found already populated ygm dependency: "
                       ${ygm_SOURCE_DIR}
        )
    else ()
        set(JUST_INSTALL_YGM ON)
        set(YGM_INSTALL ON)
        FetchContent_Populate(ygm)
        add_subdirectory(${ygm_SOURCE_DIR} ${ygm_BINARY_DIR})
        message(STATUS "Cloned ygm dependency " ${ygm_SOURCE_DIR})
    endif ()
else ()
    message(STATUS "Found installed ygm dependency " ${ygm_DIR})
endif ()

function(setup_ygm_target exe_name)
	add_executable(${exe_name} ${exe_name}.cpp)
	target_link_libraries(${exe_name} PRIVATE ygm::ygm)
	target_include_directories(${exe_name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
	target_include_directories(${exe_name} PRIVATE ${PROJECT_SOURCE_DIR}/include ${BOOST_INCLUDE_DIRS})
	target_link_libraries(${exe_name} PRIVATE Boost::json)
endfunction()

# Add the src subdirectory
add_subdirectory(src)
