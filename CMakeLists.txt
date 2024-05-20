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

add_library(primitives STATIC primitives/src/user_interaction.cpp primitives/src/deserialize_tools.cpp primitives/src/serialize_tools.cpp)
target_include_directories(primitives PUBLIC ${PROJECT_SOURCE_DIR}/primitives/include)

add_executable(hard-coding-server server/main.cpp)
add_executable(hard-coding-client client/main.cpp)



cpmaddpackage(
        NAME boost
        GIT_REPOSITORY "git@github.com:boostorg/boost.git"
        GIT_TAG "boost-1.82.0"
)

cpmaddpackage(
        NAME nlohmann_json
        GIT_REPOSITORY "git@github.com:nlohmann/json.git"
        GIT_TAG v3.11.3)

cpmaddpackage(
        NAME spdlog
        GIT_REPOSITORY "git@github.com:gabime/spdlog.git"
        GIT_TAG v1.14.1)

cpmaddpackage(
        NAME libassert
        GIT_REPOSITORY "git@github.com:jeremy-rifkin/libassert.git"
        GIT_TAG v2.0.2)

target_link_libraries(primitives nlohmann_json::nlohmann_json)
target_link_libraries(hard-coding-server PRIVATE Boost::system Boost::asio spdlog::spdlog libassert::assert primitives)
target_link_libraries(hard-coding-client PRIVATE Boost::system Boost::asio spdlog::spdlog libassert::assert primitives)

if (NOT PROJECT_IS_TOP_LEVEL)
    return()
endif ()

sl_compiler_warnings(hard-coding-server WARNINGS_AS_ERRORS)
sl_compiler_warnings(hard-coding-client WARNINGS_AS_ERRORS)
