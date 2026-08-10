cmake_minimum_required(VERSION 4.0)
project(okx_api)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj)
else ()
    add_definitions(-fPIC)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

find_package(Boost 1.88 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum REQUIRED)

# Prefer an installed package so a clean configuration works offline. Fetching
# remains an explicit fallback for developer machines without minizip-ng.
find_package(minizip-ng CONFIG QUIET)
if (NOT TARGET MINIZIP::minizip)
    option(OKX_FETCH_MINIZIP "Download minizip-ng when no installed package is available" ON)
    if (NOT OKX_FETCH_MINIZIP)
        message(FATAL_ERROR "minizip-ng not found; install it or configure with OKX_FETCH_MINIZIP=ON")
    endif ()

    include(FetchContent)
    FetchContent_Declare(
            minizip-ng
            GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng
            GIT_TAG        4.0.7
    )
    set(MZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MZ_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MZ_COMPAT ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(minizip-ng)
endif ()

include_directories(include SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

if (NOT TARGET stonky_common)
    add_subdirectory(stonky-cpp-common)
endif ()

set(HEADERS
        include/stonky/okx/okx.h
        include/stonky/okx/okx_enums.h
        include/stonky/okx/okx_models.h
        include/stonky/okx/okx_rest_client.h
        include/stonky/okx/okx_http_session.h
        include/stonky/okx/okx_event_models.h
        include/stonky/okx/okx_ws_client.h
        include/stonky/okx/okx_ws_session.h
        include/stonky/okx/okx_ws_stream_manager.h
        include/stonky/okx/okx_private_stream.h
        include/stonky/okx/okx_execution_gateway.h
        include/stonky/okx/okx_futures_exchange_connector.h
        include/stonky/okx/okx_market_data_utils.h
)

set(SOURCES
        src/okx.cpp
        src/okx_event_models.cpp
        src/okx_ws_client.cpp
        src/okx_ws_session.cpp
        src/okx_ws_stream_manager.cpp
        src/okx_private_stream.cpp
        src/okx_execution_gateway.cpp
        src/okx_models.cpp
        src/okx_rest_client.cpp
        src/okx_http_session.cpp
        src/okx_futures_exchange_connector.cpp
        src/okx_market_data_utils.cpp
        )

if (MODULE_MANAGER)
    add_library(okx_api SHARED ${SOURCES} ${HEADERS})
else ()
    add_library(okx_api STATIC ${SOURCES} ${HEADERS})
endif ()

option(ENABLE_TESTS "Build okx_api test executables" OFF)
if (ENABLE_TESTS)
    add_executable(okx_test test/main.cpp)
    target_link_libraries(okx_test PRIVATE spdlog::spdlog_header_only okx_api)

    add_executable(private_stream_smoke test/private_stream_smoke.cpp)
    target_link_libraries(private_stream_smoke PRIVATE spdlog::spdlog_header_only okx_api)

    add_executable(execution_gateway_smoke test/execution_gateway_smoke.cpp)
    target_link_libraries(execution_gateway_smoke PRIVATE spdlog::spdlog_header_only okx_api)
endif ()

target_link_libraries(okx_api PRIVATE spdlog::spdlog_header_only OpenSSL::Crypto OpenSSL::SSL stonky_common nlohmann_json::nlohmann_json MINIZIP::minizip)
