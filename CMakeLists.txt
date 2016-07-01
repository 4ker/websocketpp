
############ Setup project and cmake

# Project name
project (websocketpp)

# Minimum cmake requirement. We should require a quite recent
# cmake for the dependency find macros etc. to be up to date.
cmake_minimum_required (VERSION 2.6)

set (WEBSOCKETPP_MAJOR_VERSION 0)
set (WEBSOCKETPP_MINOR_VERSION 7)
set (WEBSOCKETPP_PATCH_VERSION 0)
set (WEBSOCKETPP_VERSION ${WEBSOCKETPP_MAJOR_VERSION}.${WEBSOCKETPP_MINOR_VERSION}.${WEBSOCKETPP_PATCH_VERSION})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
if (WIN32 AND NOT CYGWIN)
  set (DEF_INSTALL_CMAKE_DIR cmake)
else ()
  set (DEF_INSTALL_CMAKE_DIR lib/cmake/websocketpp)
endif ()
set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach (p INCLUDE CMAKE)
  set (var INSTALL_${p}_DIR)
  if (NOT IS_ABSOLUTE "${${var}}")
    set (${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif ()
endforeach ()

# Set CMake library search policy
if (COMMAND cmake_policy)
    cmake_policy (SET CMP0003 NEW)
    cmake_policy (SET CMP0005 NEW)
endif ()

# Disable unnecessary build types
set (CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "Configurations" FORCE)

# Include our cmake macros
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include (CMakeHelpers)

############ Paths

set (WEBSOCKETPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set (WEBSOCKETPP_INCLUDE ${WEBSOCKETPP_ROOT}/websocketpp)
set (WEBSOCKETPP_BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR})
set (WEBSOCKETPP_BIN ${WEBSOCKETPP_BUILD_ROOT}/bin)
set (WEBSOCKETPP_LIB ${WEBSOCKETPP_BUILD_ROOT}/lib)

# CMake install step prefix. I assume linux users want the prefix to
# be the default /usr or /usr/local so this is only adjusted on Windows.
# - Windows: Build the INSTALL project in your solution file.
# - Linux/OSX: make install.
if (MSVC)
    set (CMAKE_INSTALL_PREFIX "${WEBSOCKETPP_ROOT}/install")
endif ()

############  Build customization

# Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"
option (ENABLE_CPP11 "Build websocketpp with CPP11 features enabled." TRUE)
option (BUILD_EXAMPLES "Build websocketpp examples." FALSE)
option (BUILD_TESTS "Build websocketpp tests." FALSE)

if (BUILD_TESTS OR BUILD_EXAMPLES)

    enable_testing ()

    ############ Compiler specific setup

    set (WEBSOCKETPP_PLATFORM_LIBS "")
    set (WEBSOCKETPP_PLATFORM_TLS_LIBS "")
    set (WEBSOCKETPP_BOOST_LIBS miniboost)

    # VC9 and C++11 reasoning
    if (ENABLE_CPP11 AND MSVC AND MSVC90)
        message("* Detected Visual Studio 9 2008, disabling C++11 support.")
        set (ENABLE_CPP11 FALSE)
    endif ()

    # Detect clang. Not officially reported by cmake.
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-v" ERROR_VARIABLE CXX_VER_STDERR)
    if ("${CXX_VER_STDERR}" MATCHES ".*clang.*")
        set (CMAKE_COMPILER_IS_CLANGXX 1)
    endif ()

    # C++11 defines
    if (ENABLE_CPP11)
        if (MSVC)
            add_definitions (-D_WEBSOCKETPP_CPP11_FUNCTIONAL_)
            add_definitions (-D_WEBSOCKETPP_CPP11_SYSTEM_ERROR_)
            add_definitions (-D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_)
            add_definitions (-D_WEBSOCKETPP_CPP11_MEMORY_)
        else()
            add_definitions (-D_WEBSOCKETPP_CPP11_STL_)
        endif()
    endif ()

    # Visual studio
    if (MSVC)
        set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /Gy /GF /Ox /Ob2 /Ot /Oi /MP /arch:SSE2 /fp:fast")
        set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
        add_definitions (/W3 /wd4996 /wd4995 /wd4355)
        add_definitions (-DUNICODE -D_UNICODE)
        add_definitions (-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
        add_definitions (-DNOMINMAX)
    endif ()

    # g++
    if (CMAKE_COMPILER_IS_GNUCXX)
        set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
        set (WEBSOCKETPP_PLATFORM_TLS_LIBS ssl crypto)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        if (NOT APPLE)
            add_definitions (-DNDEBUG -Wall -Wcast-align) # todo: should we use CMAKE_C_FLAGS for these?
        endif ()

        # Try to detect version. Note: Not tested!
        execute_process (COMMAND ${CMAKE_CXX_COMPILER} "-dumpversion" OUTPUT_VARIABLE GCC_VERSION)
        if ("${GCC_VERSION}" STRGREATER "4.4.0")
            message("* C++11 support partially enabled due to GCC version ${GCC_VERSION}")
        endif ()
    endif ()

    # clang
    if (CMAKE_COMPILER_IS_CLANGXX)
        if (NOT APPLE)
            set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
        else()
            set (WEBSOCKETPP_PLATFORM_LIBS pthread)
        endif()
        set (WEBSOCKETPP_PLATFORM_TLS_LIBS ssl crypto)
        set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++0x -stdlib=libc++") # todo: is libc++ really needed here?
        if (NOT APPLE)
            add_definitions (-DNDEBUG -Wall -Wno-padded) # todo: should we use CMAKE_C_FLAGS for these?
        endif ()
    endif ()

    # OSX, can override above.
    if (APPLE)
        add_definitions (-DNDEBUG -Wall)
    endif ()

    ############ Dependencies
    if (MSVC)
        set (Boost_USE_MULTITHREADED TRUE)
        set (Boost_USE_STATIC_LIBS TRUE)
    else ()
        set (Boost_USE_MULTITHREADED FALSE)
        set (Boost_USE_STATIC_LIBS FALSE)
    endif ()
    include_directories (include)
    # link_directories (${Boost_LIBRARY_DIRS})

    find_package(OpenSSL)
    find_package(ZLIB)
endif()

############ Add projects

# Add main library
add_subdirectory (websocketpp)

# Add examples
if (BUILD_EXAMPLES)
    include_subdirs ("examples")
endif ()

# Add tests
if (BUILD_TESTS)
    include_subdirs ("test")
endif ()

print_used_build_config()

export (PACKAGE websocketpp)

configure_file (websocketpp-config.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-config.cmake" @ONLY)
configure_file (websocketpp-configVersion.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-configVersion.cmake" @ONLY)

# Install the websocketpp-config.cmake and websocketpp-configVersion.cmake
install (FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-config.cmake"
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-configVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)

