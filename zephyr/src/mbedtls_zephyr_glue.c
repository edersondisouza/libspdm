/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr glue for the vendored mbedTLS 3.6.5 used by libspdm. mbedtls
 * needs a single millisecond-resolution monotonic clock function
 * (mbedtls_ms_time). The upstream platform_util.c implementation
 * requires POSIX clock_gettime, which Zephyr's picolibc does not
 * provide; we route to k_uptime_get() and signal the override with
 * MBEDTLS_PLATFORM_MS_TIME_ALT (set as a compile definition by
 * zephyr/CMakeLists.txt).
 */

#include <zephyr/kernel.h>
#include <mbedtls/build_info.h>
#include <mbedtls/platform_time.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	return (mbedtls_ms_time_t)k_uptime_get();
}
