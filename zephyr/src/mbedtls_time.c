/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

/*
 * mbedTLS platform time hooks for the mbedTLS tree libspdm vendors.
 *
 * Zephyr has no hosted time(), so zephyr/CMakeLists.txt builds that
 * mbedTLS with MBEDTLS_PLATFORM_TIME_ALT and MBEDTLS_PLATFORM_MS_TIME_ALT:
 *  - mbedtls_ms_time() must then be defined somewhere, or linking fails;
 *    it maps to Zephyr's uptime.
 *  - mbedtls_time() becomes a pointer that mbedTLS defaults to returning
 *    0; libspdm_zephyr_set_wallclock() lets the application supply the
 *    real source without having to build against libspdm's private
 *    mbedTLS configuration.
 *
 * TODO: evaluate moving this file to upstream libspdm, next to the
 * other Zephyr os_stub files it already accepted (e.g.
 * os_stub/platform_lib/time_zephyr.c, os_stub/rnglib/rng_zephyr.c).
 * It is OS glue for libspdm's own vendored mbedTLS rather than Zephyr
 * module plumbing. This would only keep sample specific code in the fork.
 */

#include <zephyr/kernel.h>
#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>

#include <libspdm_zephyr.h>

static libspdm_zephyr_wallclock_fn wallclock_fn;

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)k_uptime_get();
}

static mbedtls_time_t wallclock_time(mbedtls_time_t *t)
{
    libspdm_zephyr_wallclock_fn fn = wallclock_fn;
    mbedtls_time_t now = (fn != NULL) ? (mbedtls_time_t)fn() : 0;

    if (t != NULL) {
        *t = now;
    }
    return now;
}

void libspdm_zephyr_set_wallclock(libspdm_zephyr_wallclock_fn fn)
{
    wallclock_fn = fn;
    mbedtls_platform_set_time(wallclock_time);
}
