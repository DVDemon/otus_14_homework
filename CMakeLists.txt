cmake_minimum_required(VERSION 3.2)

if($ENV{TRAVIS_BUILD_NUMBER})
 project(14_homework VERSION 0.0.$ENV{TRAVIS_BUILD_NUMBER})
else()
 project(14_homework VERSION 0.0.3)
endif()

#SET(CMAKE_CXX_COMPILER /usr/local/Cellar/gcc/HEAD-f2787da/bin/c++-HEAD)

find_package(Threads)
find_package(Boost COMPONENTS system regex REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(yamr main.cpp)

set_target_properties(yamr PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

target_include_directories(yamr PRIVATE "${CMAKE_BINARY_DIR}")
target_link_libraries(yamr PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})

target_compile_options(yamr PRIVATE -Wall -Wextra -pedantic -Werror )

install(TARGETS yamr RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT ddzuba@yandex.ru)
include(CPack)

