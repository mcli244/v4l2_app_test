cmake_minimum_required(VERSION 3.5.0)

set (TAGET video_demo)

include_directories(inc)

set(SRCS
	main.c
	src/bmp.c
	)

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/devlop)
set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_INSTALL_BINDIR}) 
set(LIBRARY_OUTPUT_PATH ${CMAKE_INSTALL_LIBDIR})
add_executable(${TAGET} ${SRCS})

target_link_libraries(${TAGET}
)