/*
 * Copyright Notice:
 * Copyright 2026 DMTF. All rights reserved.
 * License: BSD 3-Clause License.
 */

/*
 * Minimal SPDM application-message helpers shared by the Zephyr
 * sample apps. The handler answers "ping" with "pong" inside an
 * SPDM-secured session; the requester helper drives one such
 * round trip after a session has been established.
 */

#ifndef LIBSPDM_ZEPHYR_SAMPLE_APP_MESSAGE_H_
#define LIBSPDM_ZEPHYR_SAMPLE_APP_MESSAGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library/spdm_return_status.h"

#define SAMPLE_APP_PING "ping"
#define SAMPLE_APP_PONG "pong"
#define SAMPLE_APP_MSG_LEN 4U

/*
 * Responder-side hook compatible with libspdm_get_response_func.
 *
 * Behaviour:
 *   - is_app_message == false   -> LIBSPDM_STATUS_UNSUPPORTED_CAP, so
 *                                  libspdm falls back to its default
 *                                  handler for non-APP messages.
 *   - session_id   == NULL      -> LIBSPDM_STATUS_UNSUPPORTED_CAP;
 *                                  reject app messages outside a
 *                                  secured session.
 *   - request matches "ping"    -> copies "pong" into the response
 *                                  buffer and returns SUCCESS.
 */
libspdm_return_t sample_app_message_handler(void *spdm_context,
					    const uint32_t *session_id,
					    bool is_app_message,
					    size_t request_size,
					    const void *request,
					    size_t *response_size,
					    void *response);

/*
 * Requester-side driver: starts a KEY_EXCHANGE/FINISH session against
 * slot 0, sends "ping" as a secured app message, validates the
 * received "pong", then tears the session down. Returns 0 on success.
 */
int sample_app_message_exchange(void *spdm_context);

#endif /* LIBSPDM_ZEPHYR_SAMPLE_APP_MESSAGE_H_ */
