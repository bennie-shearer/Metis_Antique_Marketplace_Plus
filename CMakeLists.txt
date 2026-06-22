cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _ver)
string(STRIP "${_ver}" APP_VERSION)

project(Metis_Antique_Marketplace_Plus VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_SUPPRESS_REGENERATION true)

add_subdirectory(server)
