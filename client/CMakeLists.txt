cmake_minimum_required(VERSION 3.10)

project(mwe_networking_client)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(enet)
find_package(spdlog)
find_package(glad)
find_package(glfw3)
find_package(stb)
find_package(glm)
find_package(nlohmann_json)
target_link_libraries(${PROJECT_NAME} enet::enet spdlog::spdlog glad::glad glfw stb::stb glm::glm nlohmann_json::nlohmann_json)


