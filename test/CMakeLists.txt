cmake_minimum_required(VERSION 2.6)

add_executable(http_stream_impl_test http_stream_impl_test.cpp)
target_link_libraries(http_stream_impl_test ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${CMAKE_DL_LIBS})

if (WIN32)
	add_definitions(-D_WIN32_WINNT=0x0501 -DWIN32_LEAN_AND_MEAN -DBOOST_THREAD_USE_LIB)
	target_link_libraries(avhttp ws2_32)
endif()
