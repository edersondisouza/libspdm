/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr glue for the vendored mbedTLS 3.6.5 used by libspdm. mbedtls
 * needs:
 *   - mbedtls_ms_time : monotonic ms clock. Routed to k_uptime_get().
 *     Signaled with MBEDTLS_PLATFORM_MS_TIME_ALT.
 *   - mbedtls_time    : wall-clock time used by X.509 validity-period
 *     checks. Zephyr has no wall clock by default, so we return a
 *     constant timestamp that lies inside the validity window of the
 *     DMTF sample certificate chains shipped under unit_test/sample_key.
 *     Signaled with MBEDTLS_PLATFORM_TIME_ALT.
 *
 * Both flags are set as compile definitions on the libspdm
 * zephyr_library by zephyr/CMakeLists.txt.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	return (mbedtls_ms_time_t)k_uptime_get();
}

/*
 * 2025-06-15T00:00:00Z. Well inside the notBefore/notAfter window of
 * the DMTF sample ECDSA-P256 chains (notBefore ~ 2023, notAfter ~ 2033).
 * Update if the bundled sample certificates are regenerated with a
 * window that no longer contains this value.
 */
#define LIBSPDM_ZEPHYR_FAKE_WALLCLOCK_S ((mbedtls_time_t)1749945600)

static mbedtls_time_t libspdm_zephyr_fake_time(mbedtls_time_t *t)
{
	if (t != NULL) {
		*t = LIBSPDM_ZEPHYR_FAKE_WALLCLOCK_S;
	}
	return LIBSPDM_ZEPHYR_FAKE_WALLCLOCK_S;
}

static int libspdm_mbedtls_zephyr_init(void)
{
	mbedtls_platform_set_time(libspdm_zephyr_fake_time);
	return 0;
}

SYS_INIT(libspdm_mbedtls_zephyr_init, APPLICATION, 0);
