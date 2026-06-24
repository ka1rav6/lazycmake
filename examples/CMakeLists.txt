# ==========================================================================
# Examples CMakeLists.txt
#
# Build with: cmake -B build && cmake --build build
# Run with:   ./build/examples/<name>
#
# These examples require the lazycmake_core library (built above).
# ==========================================================================

add_executable(example01_basic_project_generation
    01_basic_project_generation.cpp
)
target_link_libraries(example01_basic_project_generation PRIVATE lazycmake_core)
target_compile_features(example01_basic_project_generation PRIVATE cxx_std_20)

add_executable(example02_event_bus_usage
    02_event_bus_usage.cpp
)
target_link_libraries(example02_event_bus_usage PRIVATE lazycmake_core)
target_compile_features(example02_event_bus_usage PRIVATE cxx_std_20)

add_executable(example03_config_system_demo
    03_config_system_demo.cpp
)
target_link_libraries(example03_config_system_demo PRIVATE lazycmake_core)
target_compile_features(example03_config_system_demo PRIVATE cxx_std_20)

add_executable(example04_build_pipeline_demo
    04_build_pipeline_demo.cpp
)
target_link_libraries(example04_build_pipeline_demo PRIVATE lazycmake_core)
target_compile_features(example04_build_pipeline_demo PRIVATE cxx_std_20)
