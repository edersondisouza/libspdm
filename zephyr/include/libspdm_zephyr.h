/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

/**
 * @file
 * Zephyr-specific hooks of the libspdm module.
 */

#ifndef LIBSPDM_ZEPHYR_H
#define LIBSPDM_ZEPHYR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wall-clock callback type.
 *
 * @return Seconds elapsed since 1970-01-01T00:00:00Z.
 */
typedef int64_t (*libspdm_zephyr_wallclock_fn)(void);

/**
 * Install the wall clock used by X.509 validity-period checks.
 *
 * Zephyr has no wall clock of its own, and where one comes from (an
 * RTC, SNTP, a secure time service, ...) is user's choice. Until a
 * source is installed, the time reads as 0 (the Unix epoch), so
 * certificate verification fails rather than silently accepting an
 * expired or not-yet-valid chain.
 *
 * Only available with CONFIG_LIBSPDM_CRYPTO_MBEDTLS.
 *
 * @param fn Callback returning Unix seconds, or NULL to go back to 0.
 */
void libspdm_zephyr_set_wallclock(libspdm_zephyr_wallclock_fn fn);

#ifdef __cplusplus
}
#endif

#endif /* LIBSPDM_ZEPHYR_H */
