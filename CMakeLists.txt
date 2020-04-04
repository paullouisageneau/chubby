cmake_minimum_required (VERSION 3.7)
project (chubby
	DESCRIPTION "WebRTC Visioconference hub"
	VERSION 0.0.1
	LANGUAGES CXX)

option(USE_GNUTLS "Use GnuTLS instead of OpenSSL" ON)
option(USE_JUICE "Use libjuice instead of libnice" ON)
option(USE_SRTP "Use SRTP for media" ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

set(CHUBBY_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/message.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/session.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/signaling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
add_subdirectory(deps/libdatachannel EXCLUDE_FROM_ALL)

add_executable(chubby ${CHUBBY_SOURCES})
set_target_properties(chubby PROPERTIES
	VERSION ${PROJECT_VERSION}
	CXX_STANDARD 17)
target_link_libraries(chubby LibDataChannel::LibDataChannelStatic)

