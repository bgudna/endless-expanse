add_executable(ee_tests
    test_main.cpp
    test_fixed_timestep.cpp
    test_state_stack.cpp
    test_vector.cpp
    test_camera.cpp
    test_ship.cpp
    test_world.cpp
    test_sim.cpp)
target_compile_definitions(ee_tests PRIVATE EE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
target_link_libraries(ee_tests PRIVATE ee_core doctest::doctest)
ee_enable_warnings(ee_tests)

add_test(NAME ee_tests COMMAND ee_tests)
