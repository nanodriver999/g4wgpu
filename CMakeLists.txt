cmake_minimum_required(VERSION 3.20)

project(g4wgpu VERSION 0.1.0 LANGUAGES CXX)

option(G4WGPU_BUILD_TESTING "Build g4wgpu tests" ON)
option(G4WGPU_ENABLE_WEBGPU "Enable the WebGPU backend" OFF)

add_library(g4wgpu_core
    src/core/TrackBatch.cc
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
    message(FATAL_ERROR
        "G4WGPU_ENABLE_WEBGPU is reserved for PR 3. "
        "The current core intentionally has no GPU dependency.")
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
endif()
