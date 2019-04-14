cmake_minimum_required(VERSION 3.6)
project(align_perf_test)

add_executable(${PROJECT_NAME} src/main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_14)
target_link_libraries(${PROJECT_NAME} PRIVATE -static-libstdc++)
