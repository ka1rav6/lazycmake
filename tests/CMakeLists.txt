add_executable(lazycmake_core_tests
    core/project_test.cpp
    core/target_validator_test.cpp
    core/project_repository_test.cpp
    cmake_gen/modern_cmake_generator_test.cpp
)

target_link_libraries(lazycmake_core_tests
    PRIVATE
        lazycmake_core
        Catch2::Catch2WithMain
)

target_compile_features(lazycmake_core_tests PRIVATE cxx_std_20)

include(Catch)
catch_discover_tests(lazycmake_core_tests)
