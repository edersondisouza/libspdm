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

# libspdm's own CMake (the upstream host build) suppresses
# -Wmaybe-uninitialized / -Wuninitialized because there are a number
# of known false positives in spdm_{requester,responder}_lib where the
# uninitialised path is guarded by an earlier error return. Zephyr's
# default warning profile is stricter and re-surfaces them, so mirror
# upstream's policy on this library only. Compile-options on the
# zephyr_library scope don't leak into the application.
zephyr_library_compile_options(-Wno-maybe-uninitialized -Wno-uninitialized)

# Disable PQC support across libspdm and its os_stub sample libraries
# for the initial Zephyr port. The PQC support pulls in ML-DSA / ML-KEM /
# SLH-DSA primitives that are not yet wired up to Zephyr's mbedtls
# module. Turning the master knobs off elides the dependent code at
# compile time, so the same source tree still builds without dragging
# in unimplemented symbols. PQC can be re-enabled by simply dropping
# the lines below once a PQC-aware cryptlib backend is integrated.
#
# We also override SPDM_MAX_PUBLIC_KEY_INFO_LEN below for the same
# RAM-budget reason the spec ceiling of 65535 produces a 1.5 MiB
# m_key_pair_info array in the sample device-secret library.
set(LIBSPDM_DISABLE_PQC_DEFS
  LIBSPDM_ML_DSA_44_SUPPORT=0
  LIBSPDM_ML_DSA_65_SUPPORT=0
  LIBSPDM_ML_DSA_87_SUPPORT=0
  LIBSPDM_ML_KEM_512_SUPPORT=0
  LIBSPDM_ML_KEM_768_SUPPORT=0
  LIBSPDM_ML_KEM_1024_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_128S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_128F_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_192S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_192F_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_256S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHA2_256F_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_128S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_128F_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_192S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_192F_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_256S_SUPPORT=0
  LIBSPDM_SLH_DSA_SHAKE_256F_SUPPORT=0
)

# Override SPDM_MAX_PUBLIC_KEY_INFO_LEN. The spec ceiling of 65535
# would produce a 1.5 MiB m_key_pair_info array in the sample
# device-secret library and a 64 KiB stack frame in the requester's
# GET_KEY_PAIR_INFO handler -- neither fits a typical Zephyr target.
# 1024 bytes is comfortably above any traditional asymmetric
# SubjectPublicKeyInfo we'd carry (RSA-4096 SPKI is ~550 bytes).
list(APPEND LIBSPDM_DISABLE_PQC_DEFS SPDM_MAX_PUBLIC_KEY_INFO_LEN=1024)

# Embedded-friendly libspdm sizing knobs. All are #ifndef-guarded in
# spdm_lib_config.h (or, for LIBSPDM_MAX_KEY_PAIR_COUNT, in the sample
# device-secret library's key_pair.c). Applications that need larger
# values can override them at the app level.
list(APPEND LIBSPDM_DISABLE_PQC_DEFS
  # Single SPDM session is enough for the demo and saves the bulk of
  # the per-session transcript and AEAD state (~tens of KiB).
  LIBSPDM_MAX_SESSION_COUNT=1

  # The sample device-secret library populates m_key_pair_info[] with
  # one entry per enabled traditional asym algorithm (and doubles them
  # for its multi-key example). We only enable ECDSA-P256 below, so
  # 1 primary + 1 secondary == 2 entries fits and avoids the
  # -Warray-bounds warnings that MAX_KEY_PAIR_COUNT=1 would trigger.
  LIBSPDM_MAX_KEY_PAIR_COUNT=2

  # 2 KiB is enough for an ECDSA-P256 leaf+CA chain; the libspdm
  # default of 4 KiB is sized for RSA-4096.
  LIBSPDM_MAX_CERT_CHAIN_SIZE=0x800

  # Disable traditional asym algorithms we don't use. Keeping only
  # ECDSA-P256 matches the planned mbedTLS handshake (smallest
  # crypto/cert footprint of the IETF suites) and keeps
  # m_key_pair_info[] in sync with MAX_KEY_PAIR_COUNT=2.
  LIBSPDM_RSA_SSA_2048_SUPPORT=0
  LIBSPDM_RSA_SSA_3072_SUPPORT=0
  LIBSPDM_RSA_SSA_4096_SUPPORT=0
  LIBSPDM_RSA_PSS_2048_SUPPORT=0
  LIBSPDM_RSA_PSS_3072_SUPPORT=0
  LIBSPDM_RSA_PSS_4096_SUPPORT=0
  LIBSPDM_ECDSA_P384_SUPPORT=0
  LIBSPDM_ECDSA_P521_SUPPORT=0
  LIBSPDM_SM2_DSA_P256_SUPPORT=0
  LIBSPDM_EDDSA_ED25519_SUPPORT=0
  LIBSPDM_EDDSA_ED448_SUPPORT=0
)
zephyr_library_compile_definitions(${LIBSPDM_DISABLE_PQC_DEFS})
zephyr_compile_definitions(${LIBSPDM_DISABLE_PQC_DEFS})

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

# spdm_crypt_ext_lib provides libspdm_measurement_hash_all() and a few
# other helpers that the sample device-secret library calls into.
file(GLOB LIBSPDM_CRYPTEXT_SRCS  ${LIBSPDM_DIR}/os_stub/spdm_crypt_ext_lib/*.c)

zephyr_library_sources(
  ${LIBSPDM_COMMON_SRCS}
  ${LIBSPDM_CRYPT_SRCS}
  ${LIBSPDM_SECMSG_SRCS}
  ${LIBSPDM_CRYPTEXT_SRCS}
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
  # Drop set_cert.c -- uses POSIX open/write/close to persist new certs
  # to /tmp on a hosted OS; libspdm_write_certificate_to_nvm is supplied
  # by the blob shim and just returns false on Zephyr.
  list(FILTER SECRET_LIB_SAMPLE_SRCS EXCLUDE REGEX "/set_cert\\.c$")
  zephyr_library_sources(${SECRET_LIB_SAMPLE_SRCS})

  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/src/secret_blob.c
  )
endif()

# ----------------------------------------------------------------------------
# libspdm <-> Zephyr libmctp bridge (optional)
# ----------------------------------------------------------------------------
if(CONFIG_LIBSPDM_TRANSPORT_MCTP_LIBMCTP_BRIDGE)
  zephyr_library_sources(${CMAKE_CURRENT_LIST_DIR}/src/spdm_mctp_io.c)
endif()
