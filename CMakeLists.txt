cmake_minimum_required(VERSION 3.8)
project(align_perf_test)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} -pthread)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
