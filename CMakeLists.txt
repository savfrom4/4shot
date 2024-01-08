cmake_minimum_required(VERSION 3.14)
project(4shot)

find_package(X11 REQUIRED)
find_package(PNG REQUIRED)

add_executable(4shot "main.c")
include_directories(${X11_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_link_libraries(4shot PUBLIC ${X11_LIBRARIES} ${PNG_LIBRARIES})
target_compile_options(4shot PRIVATE -Wall -Wextra -Wpedantic -Werror)

install(TARGETS 4shot RUNTIME DESTINATION bin)
