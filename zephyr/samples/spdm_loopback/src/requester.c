/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 **/

/*
 * SPDM requester thread for the libspdm Zephyr loopback demo.
 *
 * After the responder thread is up, this thread builds a libspdm
 * requester context and drives libspdm_init_connection() which
 * performs GET_VERSION + GET_CAPABILITIES + NEGOTIATE_ALGORITHMS
 * over the loopback transport. No crypto operations are needed for
 * these three steps, so the demo passes even with the null crypto
 * backend; switching to the mbedTLS backend (and embedded ECDSA-P256
 * sample certs) extends the milestone to GET_DIGESTS / GET_CERTIFICATE
 * / CHALLENGE_AUTH / GET_MEASUREMENTS without further code changes.
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "library/spdm_common_lib.h"
#include "library/spdm_requester_lib.h"
#include "hal/library/memlib.h"
#include "industry_standard/spdm.h"

#include "spdm_loopback.h"

void *requester_spdm_context;
void *requester_scratch;

static void configure_requester(void *ctx)
{
    libspdm_data_parameter_t parameter;
    uint8_t  u8;
    uint16_t u16;
    uint32_t u32;
    spdm_version_number_t version;

    libspdm_zero_mem(&parameter, sizeof(parameter));
    parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;

    version = SPDM_MESSAGE_VERSION_12 << SPDM_VERSION_NUMBER_SHIFT_BIT;
    libspdm_set_data(ctx, LIBSPDM_DATA_SPDM_VERSION, &parameter,
                     &version, sizeof(version));

    u8 = 0;
    libspdm_set_data(ctx, LIBSPDM_DATA_CAPABILITY_CT_EXPONENT, &parameter,
                     &u8, sizeof(u8));

    u32 = SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP |
          SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHAL_CAP;
    libspdm_set_data(ctx, LIBSPDM_DATA_CAPABILITY_FLAGS, &parameter,
                     &u32, sizeof(u32));

    u8 = SPDM_MEASUREMENT_SPECIFICATION_DMTF;
    libspdm_set_data(ctx, LIBSPDM_DATA_MEASUREMENT_SPEC, &parameter,
                     &u8, sizeof(u8));
    u32 = SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256;
    libspdm_set_data(ctx, LIBSPDM_DATA_BASE_ASYM_ALGO, &parameter,
                     &u32, sizeof(u32));
    u32 = SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256;
    libspdm_set_data(ctx, LIBSPDM_DATA_BASE_HASH_ALGO, &parameter,
                     &u32, sizeof(u32));
    u16 = SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1;
    libspdm_set_data(ctx, LIBSPDM_DATA_DHE_NAME_GROUP, &parameter,
                     &u16, sizeof(u16));
    u16 = SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM;
    libspdm_set_data(ctx, LIBSPDM_DATA_AEAD_CIPHER_SUITE, &parameter,
                     &u16, sizeof(u16));
    u16 = SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256;
    libspdm_set_data(ctx, LIBSPDM_DATA_REQ_BASE_ASYM_ALG, &parameter,
                     &u16, sizeof(u16));
    u16 = SPDM_ALGORITHMS_KEY_SCHEDULE_SPDM;
    libspdm_set_data(ctx, LIBSPDM_DATA_KEY_SCHEDULE, &parameter,
                     &u16, sizeof(u16));
    u8 = SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_1;
    libspdm_set_data(ctx, LIBSPDM_DATA_OTHER_PARAMS_SUPPORT, &parameter,
                     &u8, sizeof(u8));
}

void requester_thread_main(void *a, void *b, void *c)
{
    struct mock_transport *t = (struct mock_transport *)a;
    libspdm_return_t status;
    size_t scratch_size;

    ARG_UNUSED(b);
    ARG_UNUSED(c);

    /* Give the responder a moment to come up. */
    k_msleep(100);
    printk("[requester] starting\n");

    requester_spdm_context = malloc(libspdm_get_context_size());
    if (requester_spdm_context == NULL) {
        printk("[requester] ctx alloc failed\n");
        return;
    }
    libspdm_init_context(requester_spdm_context);

    mock_transport_install(requester_spdm_context, t);
    configure_requester(requester_spdm_context);

    scratch_size = libspdm_get_sizeof_required_scratch_buffer(
        requester_spdm_context);
    requester_scratch = malloc(scratch_size);
    if (requester_scratch == NULL) {
        printk("[requester] scratch alloc failed (%zu)\n", scratch_size);
        return;
    }
    libspdm_set_scratch_buffer(requester_spdm_context,
                               requester_scratch, scratch_size);

    printk("[requester] ready, scratch=%zu bytes\n", scratch_size);

    /* GET_VERSION only first -- this is the smallest amount of bytes
     * that exercises the loopback transport, the MCTP framing and
     * both libspdm state machines. */
    status = libspdm_init_connection(requester_spdm_context, true);
    if (LIBSPDM_STATUS_IS_ERROR(status)) {
        printk("[requester] GET_VERSION failed: 0x%x\n", status);
        return;
    }
    printk("[requester] GET_VERSION ok\n");

    /* Now run GET_CAPABILITIES + NEGOTIATE_ALGORITHMS. */
    status = libspdm_init_connection(requester_spdm_context, false);
    if (LIBSPDM_STATUS_IS_ERROR(status)) {
        printk("[requester] init_connection (caps+algs) failed: 0x%x\n",
               status);
        return;
    }
    printk("[requester] GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok\n");

    printk("[requester] *** SPDM handshake (version/caps/algs) PASSED ***\n");
}
