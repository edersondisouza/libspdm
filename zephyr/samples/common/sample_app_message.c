/*
 * Copyright Notice:
 * Copyright 2026 DMTF. All rights reserved.
 * License: BSD 3-Clause License.
 */

#include <string.h>

#include "library/spdm_common_lib.h"
#include "library/spdm_requester_lib.h"
#include "industry_standard/spdm.h"

#include "sample_app_message.h"

libspdm_return_t sample_app_message_handler(void *spdm_context,
					    const uint32_t *session_id,
					    bool is_app_message,
					    size_t request_size,
					    const void *request,
					    size_t *response_size,
					    void *response)
{
	(void)spdm_context;

	if (!is_app_message) {
		/* Let libspdm's default handler deal with non-APP traffic. */
		return LIBSPDM_STATUS_UNSUPPORTED_CAP;
	}
	if (session_id == NULL) {
		return LIBSPDM_STATUS_UNSUPPORTED_CAP;
	}

	if (request_size == SAMPLE_APP_MSG_LEN &&
	    memcmp(request, SAMPLE_APP_PING, SAMPLE_APP_MSG_LEN) == 0) {
		if (response == NULL || response_size == NULL) {
			return LIBSPDM_STATUS_INVALID_PARAMETER;
		}
		if (*response_size < SAMPLE_APP_MSG_LEN) {
			*response_size = SAMPLE_APP_MSG_LEN;
			return LIBSPDM_STATUS_BUFFER_TOO_SMALL;
		}
		memcpy(response, SAMPLE_APP_PONG, SAMPLE_APP_MSG_LEN);
		*response_size = SAMPLE_APP_MSG_LEN;
		return LIBSPDM_STATUS_SUCCESS;
	}

	return LIBSPDM_STATUS_UNSUPPORTED_CAP;
}

int sample_app_message_exchange(void *spdm_context)
{
	uint32_t session_id = 0;
	uint8_t heartbeat_period = 0;
	libspdm_return_t status;
	uint8_t response[SAMPLE_APP_MSG_LEN];
	size_t response_size = sizeof(response);

	status = libspdm_start_session(
		spdm_context, false /* use_psk */, NULL, 0,
		SPDM_CHALLENGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH,
		0 /* slot_id */, 0 /* session_policy */,
		&session_id, &heartbeat_period, NULL /* measurement_hash */);
	if (status != LIBSPDM_STATUS_SUCCESS) {
		/* Negative libspdm_return_t value encoded as -(status &
		 * 0xffff) so the caller can read the original 32-bit code
		 * back out by negating and masking. */
		return -(int)(status & 0xffffU);
	}

	status = libspdm_send_receive_data(
		spdm_context, &session_id, true /* is_app_message */,
		SAMPLE_APP_PING, SAMPLE_APP_MSG_LEN,
		response, &response_size);
	if (status != LIBSPDM_STATUS_SUCCESS) {
		(void)libspdm_stop_session(spdm_context, session_id, 0);
		return -(int)(status & 0xffffU);
	}

	if (response_size != SAMPLE_APP_MSG_LEN ||
	    memcmp(response, SAMPLE_APP_PONG, SAMPLE_APP_MSG_LEN) != 0) {
		(void)libspdm_stop_session(spdm_context, session_id, 0);
		return -1;
	}

	status = libspdm_stop_session(spdm_context, session_id, 0);
	if (status != LIBSPDM_STATUS_SUCCESS) {
		return -(int)(status & 0xffffU);
	}

	return 0;
}
