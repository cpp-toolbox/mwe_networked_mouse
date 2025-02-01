cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(mwe_networking_server)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(enet)
find_package(spdlog)
find_package(glm)
target_link_libraries(${PROJECT_NAME} enet::enet spdlog::spdlog glm::glm)
