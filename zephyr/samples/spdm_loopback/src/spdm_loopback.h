/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 **/

#ifndef SPDM_LOOPBACK_H
#define SPDM_LOOPBACK_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>

/*
 * Buffer sizing.
 *
 * libspdm wants a contiguous send/receive buffer per side that is
 * large enough to hold the biggest SPDM message plus the transport
 * overhead. Keep it conservative for the loopback demo so we fit
 * inside Zephyr's default malloc/heap budget on qemu_x86_64.
 */
#define SPDM_LOOPBACK_TRANSPORT_HEADER_SIZE   64
#define SPDM_LOOPBACK_TRANSPORT_TAIL_SIZE     64
#define SPDM_LOOPBACK_TRANSPORT_ADDITIONAL    (SPDM_LOOPBACK_TRANSPORT_HEADER_SIZE + \
                                               SPDM_LOOPBACK_TRANSPORT_TAIL_SIZE)
#define SPDM_LOOPBACK_BUFFER_SIZE             (0x1100 + SPDM_LOOPBACK_TRANSPORT_ADDITIONAL)

/*
 * Mock loopback transport.
 *
 * One mock_transport instance holds a TX channel and an RX channel.
 * The requester and responder each hold a pointer to one instance,
 * with their TX/RX swapped relative to each other.
 *
 * Send is non-blocking on a free slot, blocks if the previous message
 * has not been consumed yet. Receive blocks until data is available.
 */
struct mock_channel {
    uint8_t   buf[SPDM_LOOPBACK_BUFFER_SIZE];
    size_t    len;
    struct k_sem ready;   /* posted by sender after writing buf */
    struct k_sem freed;   /* posted by receiver after consuming */
};

struct mock_transport {
    struct mock_channel *tx;   /* this side writes here */
    struct mock_channel *rx;   /* this side reads from here */
};

void mock_channel_init(struct mock_channel *ch);

/* libspdm device-IO callbacks. The libspdm context is configured with
 * a pointer to a struct mock_transport via libspdm_set_data() so the
 * callbacks can find their side of the loopback. */
struct mock_transport *mock_transport_get(void *spdm_context);
void mock_transport_set(void *spdm_context, struct mock_transport *t);

/* Convenience: registers IO + MCTP framing + buffer callbacks. */
void mock_transport_install(void *spdm_context, struct mock_transport *t);

/* Roles. Each runs on its own Zephyr thread. */
void responder_thread_main(void *a, void *b, void *c);
void requester_thread_main(void *a, void *b, void *c);

#endif /* SPDM_LOOPBACK_H */
