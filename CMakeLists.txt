project(rmda-example)
cmake_minimum_required(VERSION 3.0)

include_directories(include)

add_compile_options(-g)

add_executable(ibv_test ibv_test.c setup_ibv.c util/ibv_print_info.c)
target_link_libraries(ibv_test ibverbs)

add_executable(rc_send_recv rc_send_recv.c setup_ibv.c util/ibv_print_info.c)
target_link_libraries(rc_send_recv ibverbs)