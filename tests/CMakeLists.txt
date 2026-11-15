add_executable(ee_tests
    test_main.cpp
    test_fixed_timestep.cpp
    test_state_stack.cpp
    test_vector.cpp
    test_camera.cpp
    test_ship.cpp
    test_world.cpp)
target_link_libraries(ee_tests PRIVATE ee_core doctest::doctest)
ee_enable_warnings(ee_tests)

add_test(NAME ee_tests COMMAND ee_tests)
