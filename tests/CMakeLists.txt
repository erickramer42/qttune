find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# --- Core tests: MUST stay Qt-free (Ubuntu CI job has no Qt installed) ---
add_executable(test_core test_core.cpp)

target_link_libraries(test_core PRIVATE
    qttune-core
    GTest::gtest_main
)

add_test(NAME CoreTests COMMAND test_core)

# --- Bridge tests: only when the full app (and Qt) is available ---
if(NOT QTTUNE_BUILD_CORE_ONLY)

    find_package(Qt6 COMPONENTS Core REQUIRED)

    add_executable(test_bridge
        test_bridge.cpp
        ../app/qttunebridge.cpp
    )

    set_target_properties(test_bridge PROPERTIES AUTOMOC ON)

    target_include_directories(test_bridge PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../app
    )

    target_link_libraries(test_bridge PRIVATE
        qttune-core
        Qt6::Core
        GTest::gtest
    )

    add_test(NAME BridgeTests COMMAND test_bridge)

    # Copy all runtime DLLs (Qt6Core.dll, ...) next to the exe at build time.
    # Works for manual runs AND ctest, all configs, no PATH games.
    if(WIN32)
        add_custom_command(TARGET test_bridge POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:test_bridge>
                    $<TARGET_FILE_DIR:test_bridge>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()
