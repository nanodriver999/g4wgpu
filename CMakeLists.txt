cmake_minimum_required(VERSION 3.20)

project(g4wgpu VERSION 0.1.0 LANGUAGES CXX)

option(G4WGPU_BUILD_TESTING "Build g4wgpu tests" ON)
option(G4WGPU_ENABLE_WEBGPU "Enable the WebGPU backend" OFF)
option(G4WGPU_ENABLE_GEANT4 "Enable the Geant4 integration layer" OFF)
option(G4WGPU_RUN_GPU_TESTS "Run tests that require a usable GPU/WebGPU adapter" OFF)

set(G4WGPU_WGPU_NATIVE_INCLUDE_DIR "" CACHE PATH
    "Directory containing wgpu-native wgpu.h")
set(G4WGPU_WEBGPU_HEADER_DIR "" CACHE PATH
    "Directory containing the webgpu.h version used by wgpu-native")
set(G4WGPU_WGPU_NATIVE_LIBRARY "" CACHE FILEPATH
    "Path to the wgpu-native library")

add_library(g4wgpu_core
    src/core/TrackBatch.cc
    src/core/CounterRng.cc
    src/core/BatchOps.cc
    src/core/TrackingPolicy.cc
    src/physics/KleinNishina.cc
    src/cpu/CpuBackend.cc
)

add_library(g4wgpu::core ALIAS g4wgpu_core)

target_compile_features(g4wgpu_core PUBLIC cxx_std_17)

target_include_directories(g4wgpu_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(G4WGPU_ENABLE_WEBGPU)
    if(NOT G4WGPU_WGPU_NATIVE_INCLUDE_DIR)
        find_path(G4WGPU_WGPU_NATIVE_INCLUDE_DIR
            NAMES wgpu.h
            PATH_SUFFIXES include ffi
        )
    endif()

    if(NOT G4WGPU_WEBGPU_HEADER_DIR)
        find_path(G4WGPU_WEBGPU_HEADER_DIR
            NAMES webgpu.h
            PATH_SUFFIXES include ffi/webgpu-headers webgpu-headers
        )
    endif()

    if(NOT G4WGPU_WGPU_NATIVE_LIBRARY)
        find_library(G4WGPU_WGPU_NATIVE_LIBRARY
            NAMES wgpu_native wgpu-native
        )
    endif()

    if(NOT G4WGPU_WGPU_NATIVE_INCLUDE_DIR
       OR NOT EXISTS "${G4WGPU_WGPU_NATIVE_INCLUDE_DIR}/wgpu.h")
        message(FATAL_ERROR
            "G4WGPU_ENABLE_WEBGPU=ON requires the directory containing "
            "wgpu-native's wgpu.h. Set G4WGPU_WGPU_NATIVE_INCLUDE_DIR.")
    endif()

    if(NOT G4WGPU_WEBGPU_HEADER_DIR
       OR NOT EXISTS "${G4WGPU_WEBGPU_HEADER_DIR}/webgpu.h")
        message(FATAL_ERROR
            "G4WGPU_ENABLE_WEBGPU=ON requires the webgpu.h version used "
            "by wgpu-native. Set G4WGPU_WEBGPU_HEADER_DIR.")
    endif()

    if(NOT G4WGPU_WGPU_NATIVE_LIBRARY)
        message(FATAL_ERROR
            "G4WGPU_ENABLE_WEBGPU=ON requires wgpu-native. Set "
            "G4WGPU_WGPU_NATIVE_LIBRARY to the native library path.")
    endif()

    target_sources(g4wgpu_core
        PRIVATE
            src/webgpu/WebGpuBackend.cc
    )

    target_include_directories(g4wgpu_core
        PRIVATE
            "${G4WGPU_WGPU_NATIVE_INCLUDE_DIR}"
            "${G4WGPU_WEBGPU_HEADER_DIR}"
    )

    target_link_libraries(g4wgpu_core
        PRIVATE
            "${G4WGPU_WGPU_NATIVE_LIBRARY}"
    )

    target_compile_definitions(g4wgpu_core
        PUBLIC
            G4WGPU_HAS_WEBGPU=1
    )
endif()

if(G4WGPU_ENABLE_GEANT4)
    find_package(Geant4 REQUIRED)

    add_library(g4wgpu_geant4
        src/geant4/G4WgpuTrackingManager.cc
    )
    add_library(g4wgpu::geant4 ALIAS g4wgpu_geant4)

    target_compile_features(g4wgpu_geant4 PUBLIC cxx_std_17)

    target_include_directories(g4wgpu_geant4
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${Geant4_INCLUDE_DIRS}
    )

    target_link_libraries(g4wgpu_geant4
        PUBLIC
            g4wgpu::core
        PRIVATE
            ${Geant4_LIBRARIES}
    )

    target_compile_definitions(g4wgpu_geant4
        PUBLIC
            G4WGPU_HAS_GEANT4=1
    )
endif()

if(G4WGPU_BUILD_TESTING)
    enable_testing()

    add_executable(g4wgpu_cpu_backend_test
        tests/unit/test_cpu_backend.cc
    )
    target_link_libraries(g4wgpu_cpu_backend_test PRIVATE g4wgpu::core)

    add_test(
        NAME g4wgpu.cpu_backend
        COMMAND g4wgpu_cpu_backend_test
    )

    add_executable(g4wgpu_counter_rng_test
        tests/unit/test_counter_rng.cc
    )
    target_link_libraries(g4wgpu_counter_rng_test PRIVATE g4wgpu::core)

    add_test(
        NAME g4wgpu.counter_rng
        COMMAND g4wgpu_counter_rng_test
    )

    add_executable(g4wgpu_klein_nishina_test
        tests/numerical/test_klein_nishina.cc
    )
    target_link_libraries(g4wgpu_klein_nishina_test PRIVATE g4wgpu::core)

    add_test(
        NAME g4wgpu.klein_nishina
        COMMAND g4wgpu_klein_nishina_test
    )

    add_executable(g4wgpu_tracking_policy_test
        tests/unit/test_tracking_policy.cc
    )
    target_link_libraries(g4wgpu_tracking_policy_test PRIVATE g4wgpu::core)

    add_test(
        NAME g4wgpu.tracking_policy
        COMMAND g4wgpu_tracking_policy_test
    )

    if(G4WGPU_ENABLE_WEBGPU AND G4WGPU_RUN_GPU_TESTS)
        add_executable(g4wgpu_webgpu_backend_test
            tests/integration/test_webgpu_backend.cc
        )
        target_link_libraries(g4wgpu_webgpu_backend_test PRIVATE g4wgpu::core)

        add_test(
            NAME g4wgpu.webgpu_backend
            COMMAND g4wgpu_webgpu_backend_test
        )
    endif()
endif()
