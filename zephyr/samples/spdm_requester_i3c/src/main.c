/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDM requester sample over MCTP-on-I3C.
 *
 * This sample assumes a peer SPDM responder is reachable at MCTP EID
 * PEER_EID on the I3C bus described by the board overlay. It runs the
 * first three steps of the SPDM 1.2 handshake (GET_VERSION,
 * GET_CAPABILITIES, NEGOTIATE_ALGORITHMS) and prints a single PASSED /
 * FAIL line. Extending the demo to GET_DIGESTS / GET_CERTIFICATE /
 * CHALLENGE_AUTH / GET_MEASUREMENTS is a matter of switching to
 * CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y and registering an embedded
 * certificate chain via libspdm_zephyr_secret_blob_register().
 *
 * Pair this sample with samples/spdm_responder_i3c flashed onto a
 * second board on the same I3C bus.
 */

#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <libmctp.h>
#include <zephyr/pmci/mctp/mctp_i3c_controller.h>

#include "library/spdm_common_lib.h"
#include "library/spdm_requester_lib.h"
#include "library/spdm_transport_mctp_lib.h"
#include "industry_standard/spdm.h"

#include "libspdm/zephyr/spdm_mctp_io.h"

LOG_MODULE_REGISTER(spdm_requester_i3c, LOG_LEVEL_INF);

#define LOCAL_EID 20U
#define PEER_EID  11U

/*
 * libspdm computes DataTransferSize as
 *   bridge_buffer - LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE - LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE
 * Per SPDM 1.2+ section 10.3 ("Capabilities exchange"), when the
 * peer does not advertise CHUNK_CAP the request MUST satisfy
 * DataTransferSize == MaxSPDMmsgSize. We therefore pin
 * MaxSPDMmsgSize to the same value.
 */
#define LIBSPDM_MAX_SPDM_MSG_SIZE  (CONFIG_LIBSPDM_MCTP_BUFFER_SIZE - \
				    LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE - \
				    LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE)

MCTP_I3C_CONTROLLER_DT_DEFINE(mctp_i3c_ctrl, DT_NODELABEL(mctp_i3c));

static struct spdm_mctp_io spdm_io;
static uint8_t spdm_scratch[26496] __aligned(8);

static int configure_spdm(void *spdm_ctx)
{
	libspdm_data_parameter_t param;
	spdm_version_number_t version;
	uint8_t  u8;
	uint32_t u32;
	uint16_t u16;

	memset(&param, 0, sizeof(param));
	param.location = LIBSPDM_DATA_LOCATION_LOCAL;

	/*
	 * LIBSPDM_DATA_SPDM_VERSION expects an array of
	 * spdm_version_number_t (uint16_t), with the SPDM version byte
	 * placed in the high byte (SPDM_VERSION_NUMBER_SHIFT_BIT = 8).
	 */
	version = (spdm_version_number_t)SPDM_MESSAGE_VERSION_12
		  << SPDM_VERSION_NUMBER_SHIFT_BIT;
	if (libspdm_set_data(spdm_ctx, LIBSPDM_DATA_SPDM_VERSION, &param,
			     &version, sizeof(version)) != LIBSPDM_STATUS_SUCCESS) {
		return -1;
	}

	u32 = SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP |
	      SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHAL_CAP;
	if (libspdm_set_data(spdm_ctx, LIBSPDM_DATA_CAPABILITY_FLAGS, &param,
			     &u32, sizeof(u32)) != LIBSPDM_STATUS_SUCCESS) {
		return -1;
	}

	u8 = SPDM_MEASUREMENT_SPECIFICATION_DMTF;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_MEASUREMENT_SPEC,
			       &param, &u8, sizeof(u8));

	u32 = SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_BASE_ASYM_ALGO, &param,
			       &u32, sizeof(u32));

	u32 = SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_BASE_HASH_ALGO, &param,
			       &u32, sizeof(u32));

	u16 = SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_DHE_NAME_GROUP, &param,
			       &u16, sizeof(u16));

	u16 = SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_AEAD_CIPHER_SUITE,
			       &param, &u16, sizeof(u16));

	u16 = SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_REQ_BASE_ASYM_ALG,
			       &param, &u16, sizeof(u16));

	u16 = SPDM_ALGORITHMS_KEY_SCHEDULE_SPDM;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_KEY_SCHEDULE, &param,
			       &u16, sizeof(u16));

	u8 = SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_1;
	(void)libspdm_set_data(spdm_ctx, LIBSPDM_DATA_OTHER_PARAMS_SUPPORT,
			       &param, &u8, sizeof(u8));

	return 0;
}

int main(void)
{
	struct mctp *mctp_ctx;
	void *spdm_ctx;
	size_t ctx_size;
	libspdm_return_t status;
	int rc;

	LOG_INF("SPDM requester (libspdm + libmctp + I3C) on %s",
		CONFIG_BOARD_TARGET);
	LOG_INF("local EID=%u, peer EID=%u", LOCAL_EID, PEER_EID);

	mctp_ctx = mctp_init();
	__ASSERT_NO_MSG(mctp_ctx != NULL);
	mctp_register_bus(mctp_ctx, &mctp_i3c_ctrl.binding, LOCAL_EID);

	rc = spdm_mctp_io_init(&spdm_io, mctp_ctx, LOCAL_EID, PEER_EID);
	if (rc != 0) {
		LOG_ERR("spdm_mctp_io_init failed: %d", rc);
		return rc;
	}

	ctx_size = libspdm_get_context_size();
	spdm_ctx = k_malloc(ctx_size);
	__ASSERT_NO_MSG(spdm_ctx != NULL);

	libspdm_init_context(spdm_ctx);

	rc = spdm_mctp_io_register(spdm_ctx, &spdm_io);
	if (rc != 0) {
		LOG_ERR("spdm_mctp_io_register failed: %d", rc);
		return rc;
	}

	libspdm_register_transport_layer_func(spdm_ctx,
		LIBSPDM_MAX_SPDM_MSG_SIZE,
		LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE,
		LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE,
		libspdm_transport_mctp_encode_message,
		libspdm_transport_mctp_decode_message);

	libspdm_set_scratch_buffer(spdm_ctx, spdm_scratch, sizeof(spdm_scratch));

	if (configure_spdm(spdm_ctx) != 0) {
		LOG_ERR("configure_spdm failed");
		return -1;
	}

	LOG_INF("issuing GET_VERSION");
	status = libspdm_init_connection(spdm_ctx, true);
	if (status != LIBSPDM_STATUS_SUCCESS) {
		LOG_ERR("GET_VERSION failed: 0x%x", status);
		return -1;
	}
	LOG_INF("GET_VERSION ok");

	LOG_INF("issuing GET_CAPABILITIES + NEGOTIATE_ALGORITHMS");
	status = libspdm_init_connection(spdm_ctx, false);
	if (status != LIBSPDM_STATUS_SUCCESS) {
		LOG_ERR("CAPABILITIES/ALGORITHMS failed: 0x%x", status);
		return -1;
	}
	LOG_INF("GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok");

	LOG_INF("*** SPDM handshake (version/caps/algs) PASSED ***");

	return 0;
}
