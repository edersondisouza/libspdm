# Copyright Notice:
# Copyright 2026 DMTF. All rights reserved.
# License: BSD 3-Clause License.
#
# Zephyr module glue for libspdm.
#
# This file is consumed by Zephyr's build system (cmake: zephyr in
# module.yml). It builds a single zephyr_library named "libspdm" that
# contains the libspdm sources, os_stub HAL backends, the cryptlib
# (mbedtls or null) and the requested transports. Selection is driven
# entirely by Kconfig (see Kconfig and Kconfig.mbedtls).

if(NOT CONFIG_LIBSPDM)
  return()
endif()

set(LIBSPDM_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

zephyr_library_named(libspdm)

zephyr_library_include_directories(
  ${LIBSPDM_DIR}/include
  ${LIBSPDM_DIR}/include/hal
  ${LIBSPDM_DIR}/os_stub/include
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# Expose the public headers to anything that links against the module.
zephyr_include_directories(
  ${LIBSPDM_DIR}/include
  ${LIBSPDM_DIR}/include/hal
  ${LIBSPDM_DIR}/os_stub/include
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# Relax warnings that libspdm's source style trips on under Zephyr's
# stricter defaults. These are scoped to this library only.
zephyr_library_compile_options(
  -Wno-vla
  -Wno-unused-parameter
  -Wno-pointer-sign
  -Wno-incompatible-pointer-types
)

# ----------------------------------------------------------------------------
# Core libspdm sources (always needed)
# ----------------------------------------------------------------------------
file(GLOB LIBSPDM_COMMON_SRCS    ${LIBSPDM_DIR}/library/spdm_common_lib/*.c)
file(GLOB LIBSPDM_CRYPT_SRCS     ${LIBSPDM_DIR}/library/spdm_crypt_lib/*.c)
file(GLOB LIBSPDM_SECMSG_SRCS    ${LIBSPDM_DIR}/library/spdm_secured_message_lib/*.c)

zephyr_library_sources(
  ${LIBSPDM_COMMON_SRCS}
  ${LIBSPDM_CRYPT_SRCS}
  ${LIBSPDM_SECMSG_SRCS}
)

if(CONFIG_LIBSPDM_REQUESTER)
  file(GLOB LIBSPDM_REQ_SRCS ${LIBSPDM_DIR}/library/spdm_requester_lib/*.c)
  zephyr_library_sources(${LIBSPDM_REQ_SRCS})
endif()

if(CONFIG_LIBSPDM_RESPONDER)
  file(GLOB LIBSPDM_RSP_SRCS ${LIBSPDM_DIR}/library/spdm_responder_lib/*.c)
  zephyr_library_sources(${LIBSPDM_RSP_SRCS})
endif()

# ----------------------------------------------------------------------------
# Transports (each is a thin codec; pick whichever the app registers)
# ----------------------------------------------------------------------------
if(CONFIG_LIBSPDM_TRANSPORT_MCTP)
  file(GLOB LIBSPDM_MCTP_SRCS ${LIBSPDM_DIR}/library/spdm_transport_mctp_lib/*.c)
  zephyr_library_sources(${LIBSPDM_MCTP_SRCS})
endif()

if(CONFIG_LIBSPDM_TRANSPORT_PCI_DOE)
  file(GLOB LIBSPDM_DOE_SRCS ${LIBSPDM_DIR}/library/spdm_transport_pcidoe_lib/*.c)
  zephyr_library_sources(${LIBSPDM_DOE_SRCS})
endif()

if(CONFIG_LIBSPDM_TRANSPORT_TCP)
  file(GLOB LIBSPDM_TCP_SRCS ${LIBSPDM_DIR}/library/spdm_transport_tcp_lib/*.c)
  zephyr_library_sources(${LIBSPDM_TCP_SRCS})
endif()

if(CONFIG_LIBSPDM_TRANSPORT_STORAGE)
  file(GLOB LIBSPDM_STG_SRCS ${LIBSPDM_DIR}/library/spdm_transport_storage_lib/*.c)
  zephyr_library_sources(${LIBSPDM_STG_SRCS})
endif()

# ----------------------------------------------------------------------------
# OS HAL backends (Zephyr-flavoured; landed in Phase 1)
# ----------------------------------------------------------------------------
zephyr_library_sources(
  ${LIBSPDM_DIR}/os_stub/memlib/compare_mem.c
  ${LIBSPDM_DIR}/os_stub/memlib/copy_mem.c
  ${LIBSPDM_DIR}/os_stub/memlib/set_mem.c
  ${LIBSPDM_DIR}/os_stub/memlib/zero_mem.c
  ${LIBSPDM_DIR}/os_stub/malloclib/malloclib_zephyr.c
  ${LIBSPDM_DIR}/os_stub/platform_lib/time_zephyr.c
  ${LIBSPDM_DIR}/os_stub/platform_lib/watchdog.c
  ${LIBSPDM_DIR}/os_stub/rnglib/rng_zephyr.c
)

if(CONFIG_LIBSPDM_DEBUG_ENABLE)
  zephyr_library_sources(${LIBSPDM_DIR}/os_stub/debuglib/debuglib_zephyr.c)
else()
  zephyr_library_sources(${LIBSPDM_DIR}/os_stub/debuglib_null/debuglib.c)
endif()

# ----------------------------------------------------------------------------
# Crypto backend
# ----------------------------------------------------------------------------
if(CONFIG_LIBSPDM_CRYPTO_MBEDTLS)
  # Reuse libspdm's cryptlib_mbedtls, but exclude the sys_call/ wrappers:
  # on Zephyr we let mbedtls use libc malloc directly and we never call
  # the my_printf/my_snprintf stubs (no MBEDTLS_PLATFORM_*_ALT enabled).
  file(GLOB_RECURSE CRYPTLIB_MBEDTLS_SRCS
       ${LIBSPDM_DIR}/os_stub/cryptlib_mbedtls/*.c)
  list(FILTER CRYPTLIB_MBEDTLS_SRCS EXCLUDE REGEX "/sys_call/")
  # Skip PQC sources for now (require ML-DSA / ML-KEM in Zephyr mbedtls).
  list(FILTER CRYPTLIB_MBEDTLS_SRCS EXCLUDE REGEX "_pqc\\.c$")
  list(FILTER CRYPTLIB_MBEDTLS_SRCS EXCLUDE REGEX "(mldsa|mlkem|slhdsa)")
  zephyr_library_sources(${CRYPTLIB_MBEDTLS_SRCS})

  zephyr_library_include_directories(
    ${LIBSPDM_DIR}/os_stub/cryptlib_mbedtls
  )

  # mbedtls library target is provided by Zephyr's mbedtls module.
  zephyr_library_link_libraries(mbedTLS)
elseif(CONFIG_LIBSPDM_CRYPTO_NULL)
  file(GLOB_RECURSE CRYPTLIB_NULL_SRCS
       ${LIBSPDM_DIR}/os_stub/cryptlib_null/*.c)
  zephyr_library_sources(${CRYPTLIB_NULL_SRCS})
  zephyr_library_include_directories(
    ${LIBSPDM_DIR}/os_stub/cryptlib_null
  )
endif()

# ----------------------------------------------------------------------------
# Sample device-secret library (Zephyr blob-backed)
# ----------------------------------------------------------------------------
if(CONFIG_LIBSPDM_SECRET_LIB_SAMPLE)
  # The sample lib itself is portable; the file I/O routine it relies on
  # (libspdm_read_input_file) is provided by our Zephyr blob shim instead
  # of by spdm-emu's POSIX support.c.
  file(GLOB SECRET_LIB_SAMPLE_SRCS
       ${LIBSPDM_DIR}/os_stub/spdm_device_secret_lib_sample/*.c)
  # Drop the PQC-only readers; they reference algorithms we excluded above.
  list(FILTER SECRET_LIB_SAMPLE_SRCS EXCLUDE REGEX "_pqc\\.c$")
  zephyr_library_sources(${SECRET_LIB_SAMPLE_SRCS})

  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/src/secret_blob.c
  )
endif()
