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

sl_target_link_system_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
