cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" _ver)
string(STRIP "${_ver}" APP_VERSION)

project(Metis_Antique_Marketplace_Plus VERSION ${APP_VERSION} LANGUAGES CXX C)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# OpenSSL detection
# ---------------------------------------------------------------------------
if(DEFINED ENV{OPENSSL_ROOT_DIR})
    set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../openssl-prebuilt/windows")
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../openssl-prebuilt/windows")
elseif(EXISTS "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
endif()

find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    add_compile_definitions(METIS_TLS=1)
    message(STATUS "TLS enabled: OpenSSL ${OPENSSL_VERSION}")
else()
    message(STATUS "TLS disabled: OpenSSL not found")
endif()

# ---------------------------------------------------------------------------
# Third-party paths
# ---------------------------------------------------------------------------
set(SQLITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sqlite")
set(JSON_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/json")
set(BCRYPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/bcrypt")

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/router.cpp
    src/logger.cpp
    src/db.cpp
    src/pson.cpp
    src/util.cpp
    src/app_items.cpp
    src/app_listings.cpp
    src/app_sales.cpp
    src/app_appraisals.cpp
    src/app_market.cpp
    src/app_auth.cpp
    src/app_photos.cpp
    src/app_comments.cpp
    src/app_audit.cpp
    src/app_purchases.cpp
    src/ws_log.cpp
    src/smtp.cpp
    src/app_business.cpp
    src/app_pos.cpp
    src/app_sync.cpp
    "${SQLITE_DIR}/sqlite3.c"
    "${BCRYPT_DIR}/bcrypt.c"
    "${BCRYPT_DIR}/crypt_blowfish/crypt_blowfish.c"
    "${BCRYPT_DIR}/crypt_blowfish/crypt_gensalt.c"
    "${BCRYPT_DIR}/crypt_blowfish/wrapper.c"
)

add_executable(metis_antique_plus ${SOURCES})

target_include_directories(metis_antique_plus PRIVATE
    src
    "${SQLITE_DIR}"
    "${JSON_DIR}"
    "${BCRYPT_DIR}"
    "${BCRYPT_DIR}/crypt_blowfish"
)

if(OPENSSL_FOUND)
    target_include_directories(metis_antique_plus PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(metis_antique_plus PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(WIN32)
    target_link_libraries(metis_antique_plus PRIVATE ws2_32 mswsock crypt32)
endif()

target_compile_definitions(metis_antique_plus PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
)

# Win32: prevent wincrypt.h/ncrypt.h pulling in before bcrypt.h in include path
if(WIN32)
    target_compile_definitions(metis_antique_plus PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
    )
endif()

# Suppress warnings from third-party C code
set_source_files_properties(
    "${SQLITE_DIR}/sqlite3.c"
    "${BCRYPT_DIR}/bcrypt.c"
    "${BCRYPT_DIR}/crypt_blowfish/crypt_blowfish.c"
    "${BCRYPT_DIR}/crypt_blowfish/crypt_gensalt.c"
    "${BCRYPT_DIR}/crypt_blowfish/wrapper.c"
    PROPERTIES COMPILE_FLAGS "-w"
)

# ---------------------------------------------------------------------------
# Web assets sync
# ---------------------------------------------------------------------------
add_custom_target(sync_web_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../web"
        "${CMAKE_CURRENT_BINARY_DIR}/web"
    COMMENT "Syncing web assets"
)
add_dependencies(metis_antique_plus sync_web_assets)

add_custom_command(TARGET metis_antique_plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../config.pson"
        "$<TARGET_FILE_DIR:metis_antique_plus>/config.pson"
    COMMENT "Copying config.pson"
)

# ---------------------------------------------------------------------------
# TLS certificate -- auto-generate with SAN if missing or if openssl changed
# Uses the same OpenSSL found by find_package, so -addext is always available
# ---------------------------------------------------------------------------
if(OPENSSL_FOUND)
    set(CERT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../certs")
    set(CERT_FILE "${CERT_DIR}/server.crt")
    set(KEY_FILE  "${CERT_DIR}/server.key")
    set(SAN_CFG   "${CERT_DIR}/san.cfg")

    # Write the SAN config file at configure time
    file(WRITE "${SAN_CFG}"
"[req]\n\
distinguished_name = req_dn\n\
x509_extensions = v3_req\n\
prompt = no\n\
\n\
[req_dn]\n\
C = US\n\
ST = Idaho\n\
L = Coeur dAlene\n\
O = Metis\n\
CN = localhost\n\
\n\
[v3_req]\n\
subjectAltName = DNS:localhost,IP:127.0.0.1\n\
basicConstraints = CA:TRUE\n\
keyUsage = digitalSignature,keyCertSign\n\
extendedKeyUsage = serverAuth\n\
")

    # Find the openssl executable next to the OpenSSL libraries
    get_filename_component(OPENSSL_BIN_DIR "${OPENSSL_SSL_LIBRARY}" DIRECTORY)
    find_program(OPENSSL_EXE
        NAMES openssl openssl.exe
        HINTS "${OPENSSL_BIN_DIR}"
              "${OPENSSL_BIN_DIR}/../bin"
              "${OPENSSL_ROOT_DIR}/bin"
        NO_DEFAULT_PATH
    )
    if(NOT OPENSSL_EXE)
        find_program(OPENSSL_EXE NAMES openssl openssl.exe)
    endif()

    if(OPENSSL_EXE)
        message(STATUS "TLS cert tool: ${OPENSSL_EXE}")
        # Generate cert if it doesn't exist
        if(NOT EXISTS "${CERT_FILE}" OR NOT EXISTS "${KEY_FILE}")
            message(STATUS "Generating TLS certificate with SAN...")
            execute_process(
                COMMAND "${OPENSSL_EXE}" req -x509
                    -newkey rsa:2048
                    -keyout "${KEY_FILE}"
                    -out    "${CERT_FILE}"
                    -days   825
                    -nodes
                    -config "${SAN_CFG}"
                    -extensions v3_req
                RESULT_VARIABLE CERT_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
            )
            if(CERT_RESULT EQUAL 0)
                message(STATUS "TLS certificate generated: ${CERT_FILE}")
            else()
                message(WARNING "TLS certificate generation failed (result=${CERT_RESULT})")
            endif()
        else()
            message(STATUS "TLS certificate exists: ${CERT_FILE}")
        endif()
    else()
        message(WARNING "openssl executable not found -- skipping cert generation")
    endif()

    # Post-build: copy fresh cert to build output
    add_custom_command(TARGET metis_antique_plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CERT_DIR}"
            "$<TARGET_FILE_DIR:metis_antique_plus>/certs"
        COMMENT "Copying certs to build output"
    )

    # Generate a trust_cert.cmd batch file in the build output
    # Double-click it as Administrator once to trust the cert
    if(WIN32)
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/../trust_cert.cmd"
"@echo off\r\necho Trusting Metis Antique Marketplace Plus TLS certificate...\r\ncertutil -delstore Root localhost\r\ncertutil -addstore Root \"%~dp0cmake-build-release\\server\\certs\\server.crt\"\r\necho Done. Restart Edge and go to https://localhost:8481\r\npause\r\n")

        add_custom_command(TARGET metis_antique_plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../trust_cert.cmd"
                "$<TARGET_FILE_DIR:metis_antique_plus>/trust_cert.cmd"
            COMMENT "Copying trust_cert.cmd"
        )
    endif()
endif()

add_custom_command(TARGET metis_antique_plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../web/images"
        "$<TARGET_FILE_DIR:metis_antique_plus>/web/images"
    COMMENT "Copying images"
)

add_custom_command(TARGET metis_antique_plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../trust_cert.cmd"
        "$<TARGET_FILE_DIR:metis_antique_plus>/trust_cert.cmd"
    COMMENT "Copying trust_cert.cmd"
)
