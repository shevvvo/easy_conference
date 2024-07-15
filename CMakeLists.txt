cmake_minimum_required(VERSION 3.21)

include(FetchContent)
FetchContent_Declare(
        serious-cmake-library
        GIT_REPOSITORY git@github.com:UsatiyNyan/serious-cmake-library.git
        GIT_TAG main
)

FetchContent_MakeAvailable(serious-cmake-library)

sl_project_prologue(
        CXX_STANDARD 20
        C_STANDARD 17)


project(
        hard-coding
        VERSION 0.0.1
        DESCRIPTION "hard-coding"
        HOMEPAGE_URL "TODO"
        LANGUAGES C CXX)

add_library(${PROJECT_NAME} STATIC primitives/src/deserialize_tools.cpp primitives/src/serialize_tools.cpp server/src/server.cpp client/src/client.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/primitives/include ${PROJECT_SOURCE_DIR}/server/include client/include)
sl_target_link_system_libraries(${PROJECT_NAME} PRIVATE Boost::system Boost::asio spdlog::spdlog libassert::assert)

add_subdirectory(dependencies)

add_executable(hard-coding-server server/main.cpp)
add_executable(hard-coding-client client/main.cpp)

sl_target_link_system_libraries(hard-coding-server PRIVATE Boost::system Boost::asio spdlog::spdlog libassert::assert ${PROJECT_NAME})
sl_target_link_system_libraries(hard-coding-client PRIVATE Boost::system Boost::asio spdlog::spdlog libassert::assert ${PROJECT_NAME})

if (NOT PROJECT_IS_TOP_LEVEL)
    return()
endif ()

sl_compiler_warnings(${PROJECT_NAME} WARNINGS_AS_ERRORS)
sl_compiler_warnings(hard-coding-server WARNINGS_AS_ERRORS)
sl_compiler_warnings(hard-coding-client WARNINGS_AS_ERRORS)
