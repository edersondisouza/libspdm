/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 *
 *  Sample glue carrying libspdm's MCTP transport over Zephyr's libmctp.
 *
 *  Registration installs the device IO hooks, the device buffer hooks
 *  *and* libspdm's MCTP transport codec, so the application does not
 *  have to repeat the transport wiring.
 **/

#ifndef SAMPLE_MCTP_IO_H
#define SAMPLE_MCTP_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "library/spdm_common_lib.h"
#include "library/spdm_transport_mctp_lib.h"

struct mctp;

/* Size of the sender/receiver buffers of each instance. Sized for
 * GET_CERTIFICATE with the samples' ~1.5 KiB ECDSA-P256 chain; must fit
 * the largest SPDM message plus the MCTP transport framing.
 */
#define SAMPLE_MCTP_IO_BUFFER_SIZE 2048

/* In-flight RX slots between the libmctp rx callback and
 * device_receive_message(); one is enough for strict request/response.
 */
#define SAMPLE_MCTP_IO_RX_QUEUE_DEPTH 1

/* Number of (libspdm context, MCTP IO) associations tracked at once. */
#define SAMPLE_MCTP_IO_MAX_CONTEXTS 1

/* Size of one RX pool block. k_mem_slab requires both the block size
 * and the backing buffer to be word-aligned.
 */
#define SAMPLE_MCTP_IO_RX_BLOCK_SIZE \
    ROUND_UP(SAMPLE_MCTP_IO_BUFFER_SIZE, sizeof(void *))

/* Largest SPDM message that fits in one of these buffers once libspdm's MCTP
 * transport has added its framing.
 */
#define SAMPLE_MCTP_IO_MAX_SPDM_MSG_SIZE           \
    (SAMPLE_MCTP_IO_BUFFER_SIZE -                  \
     LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE -               \
     LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE)

BUILD_ASSERT(SAMPLE_MCTP_IO_BUFFER_SIZE >
         LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE +
         LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE,
         "SAMPLE_MCTP_IO_BUFFER_SIZE must exceed the MCTP transport framing");

/* Sanity cap on the RX pool. Both factors are Kconfig ints with a
 * declared range, so the product below is bounded and evaluated at
 * compile time.
 */
#define SAMPLE_MCTP_IO_RX_POOL_SIZE \
    (SAMPLE_MCTP_IO_RX_QUEUE_DEPTH * SAMPLE_MCTP_IO_RX_BLOCK_SIZE)

BUILD_ASSERT(SAMPLE_MCTP_IO_RX_POOL_SIZE <= 128 * 1024,
         "RX pool (SAMPLE_MCTP_IO_RX_QUEUE_DEPTH * "
         "SAMPLE_MCTP_IO_BUFFER_SIZE) is implausibly large");

/* One queue entry. The message payload itself stays in the RX pool
 * block p buf points at -- only this descriptor travels through the
 * queue -- together with the addressing context needed to answer it.
 * The receiver owns the block and must return it to the pool once
 * drained.
 */
struct sample_mctp_io_rx_desc {
    void *buf;
    size_t len;
    uint8_t src_eid;
    uint8_t msg_tag;
    bool tag_owner;
};

struct sample_mctp_io {
    struct mctp *mctp_ctx;
    /* EID addressed by requests this node originates. */
    uint8_t peer_eid;

    /* Addressing context of the request currently being answered. */
    bool reply_pending;
    uint8_t reply_eid;
    uint8_t reply_tag;

    uint8_t send_buf[SAMPLE_MCTP_IO_BUFFER_SIZE];
    bool send_buf_in_use;

    /* RX pool. The libmctp rx callback copies an inbound message
     * straight into a block taken from here and queues only a
     * descriptor, so no message-sized buffer is ever placed on a
     * stack.
     */
    struct k_mem_slab rx_slab;
    uint8_t rx_pool[SAMPLE_MCTP_IO_RX_POOL_SIZE] __aligned(sizeof(void *));

    struct k_msgq rx_msgq;
    char rx_msgq_storage[SAMPLE_MCTP_IO_RX_QUEUE_DEPTH *
                 sizeof(struct sample_mctp_io_rx_desc)];

    uint8_t recv_buf[SAMPLE_MCTP_IO_BUFFER_SIZE];
    bool recv_buf_in_use;
};

/**
 * @brief Initialise an MCTP IO instance.
 *
 * The libmctp context must already have been created via mctp_init()
 * and a binding registered against it via mctp_register_bus(). This
 * call wires up the rx-all hook so received messages are pushed onto
 * the instance's internal queue. It does NOT take ownership of the
 * mctp context (the application keeps it for the lifetime of the
 * binding).
 *
 * @param io        Storage for the instance.
 * @param mctp_ctx  Initialised libmctp instance.
 * @param peer_eid  EID this node addresses when it originates a
 *                  request.
 *
 * @return 0 on success, negative errno on failure.
 */
int sample_mctp_io_init(struct sample_mctp_io *io,
                struct mctp *mctp_ctx, uint8_t peer_eid);

/**
 * @brief Register this MCTP IO on a libspdm context.
 *
 * Installs its device IO and device buffer hooks, and registers
 * libspdm's MCTP transport codec.
 * Registering a context that already has one replaces the previous
 * association.
 *
 * @param spdm_context The libspdm context to drive.
 * @param io           The instance, previously initialised.
 *
 * @return 0 on success, negative errno if the (spdm_context, io)
 *         association can't be stored
 */
int sample_mctp_io_register(void *spdm_context,
                    struct sample_mctp_io *io);

/**
 * @brief Drop the association for a libspdm context.
 *
 * Frees the map slot so the instance can be re-registered or the
 * context torn down. The libspdm context keeps pointing at the
 * hooks, so it must not be used for further messaging afterwards.
 *
 * @param spdm_context The libspdm context to detach.
 *
 * @return 0 on success, -ENOENT if the context was not registered.
 */
int sample_mctp_io_unregister(void *spdm_context);

#endif /* SAMPLE_MCTP_IO_H */
