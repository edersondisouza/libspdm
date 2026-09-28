/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

/**
 * @file
 * Declarations for the application-supplied libspdm hooks shared by the
 * Zephyr samples.
 */

#ifndef SAMPLE_SUPPORT_H
#define SAMPLE_SUPPORT_H

#include <stdint.h>

#include <base.h>
#include "internal/libspdm_device_secret_lib.h"

/**
 * Point libspdm's X.509 validity checks at a usable wall clock.
 *
 * Installs the source with libspdm_zephyr_set_wallclock():
 *
 *   - with an RTC (CONFIG_RTC and an "rtc" devicetree alias), sets it
 *     to @p unix_seconds through Zephyr's RTC API
 *   - without one, a source that simply reports @p unix_seconds.
 *
 * Either way the samples run off a constant.
 *
 * @param unix_seconds Seconds since 1970-01-01T00:00:00Z to report.
 *
 * @retval 0 on success, negative errno on failure.
 */
int sample_wallclock_init(int64_t unix_seconds);

#endif /* SAMPLE_SUPPORT_H */
