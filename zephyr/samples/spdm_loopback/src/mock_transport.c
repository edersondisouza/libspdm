/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 **/

/*
 * In-process loopback transport for the libspdm Zephyr demo.
 *
 * The libspdm device_send / device_receive callbacks below receive
 * already-framed transport bytes (MCTP + SPDM) and shuttle them
 * across two k_sem-synchronised buffers - no real wire involved.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "library/spdm_common_lib.h"

#include "spdm_loopback.h"

#define MOCK_TIMEOUT_MS 5000

/* libspdm doesn't have a generic "opaque per-context pointer" data
 * key, so we keep a small map from spdm_context -> mock_transport.
 * Two slots are enough for the loopback demo (one per role). */
struct ctx_slot {
    void *spdm_context;
    struct mock_transport *transport;
};

#define CTX_MAP_SIZE 4
static struct ctx_slot ctx_map[CTX_MAP_SIZE];
static struct k_spinlock ctx_lock;

void mock_channel_init(struct mock_channel *ch)
{
    ch->len = 0;
    k_sem_init(&ch->ready, 0, 1);
    k_sem_init(&ch->freed, 1, 1);  /* slot starts empty */
}

void mock_transport_set(void *spdm_context, struct mock_transport *t)
{
    k_spinlock_key_t key = k_spin_lock(&ctx_lock);

    for (int i = 0; i < CTX_MAP_SIZE; i++) {
        if (ctx_map[i].spdm_context == NULL) {
            ctx_map[i].spdm_context = spdm_context;
            ctx_map[i].transport = t;
            k_spin_unlock(&ctx_lock, key);
            return;
        }
    }
    k_spin_unlock(&ctx_lock, key);
    printk("mock_transport: ctx map full\n");
}

struct mock_transport *mock_transport_get(void *spdm_context)
{
    k_spinlock_key_t key = k_spin_lock(&ctx_lock);

    for (int i = 0; i < CTX_MAP_SIZE; i++) {
        if (ctx_map[i].spdm_context == spdm_context) {
            struct mock_transport *t = ctx_map[i].transport;
            k_spin_unlock(&ctx_lock, key);
            return t;
        }
    }
    k_spin_unlock(&ctx_lock, key);
    return NULL;
}

/* ---------------------------------------------------------------- */
/* libspdm device_io callbacks                                      */
/* ---------------------------------------------------------------- */

static libspdm_return_t mock_send_message(void *spdm_context,
                                          size_t message_size,
                                          const void *message,
                                          uint64_t timeout)
{
    struct mock_transport *t = mock_transport_get(spdm_context);

    if (t == NULL || t->tx == NULL) {
        return LIBSPDM_STATUS_SEND_FAIL;
    }
    if (message_size > sizeof(t->tx->buf)) {
        printk("mock_send: msg %zu > buf %zu\n", message_size,
               sizeof(t->tx->buf));
        return LIBSPDM_STATUS_SEND_FAIL;
    }

    /* Wait for the slot to be drained by the peer. */
    if (k_sem_take(&t->tx->freed, K_MSEC(MOCK_TIMEOUT_MS)) != 0) {
        return LIBSPDM_STATUS_SEND_FAIL;
    }
    memcpy(t->tx->buf, message, message_size);
    t->tx->len = message_size;
    k_sem_give(&t->tx->ready);
    return LIBSPDM_STATUS_SUCCESS;
}

static libspdm_return_t mock_receive_message(void *spdm_context,
                                             size_t *message_size,
                                             void **message,
                                             uint64_t timeout)
{
    struct mock_transport *t = mock_transport_get(spdm_context);

    if (t == NULL || t->rx == NULL) {
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }

    if (k_sem_take(&t->rx->ready, K_MSEC(MOCK_TIMEOUT_MS)) != 0) {
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }
    if (t->rx->len > *message_size) {
        printk("mock_recv: msg %zu > caller buf %zu\n", t->rx->len,
               *message_size);
        k_sem_give(&t->rx->freed);
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }
    memcpy(*message, t->rx->buf, t->rx->len);
    *message_size = t->rx->len;
    t->rx->len = 0;
    k_sem_give(&t->rx->freed);
    return LIBSPDM_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------- */
/* per-context send/receive buffer                                  */
/* ---------------------------------------------------------------- */

struct sr_buffer {
    uint8_t  bytes[SPDM_LOOPBACK_BUFFER_SIZE];
    bool     acquired;
};

#define SR_MAP_SIZE CTX_MAP_SIZE
static struct sr_buffer sr_buffers[SR_MAP_SIZE];
static struct ctx_slot  sr_map[SR_MAP_SIZE];

static struct sr_buffer *find_sr(void *spdm_context, bool allocate)
{
    for (int i = 0; i < SR_MAP_SIZE; i++) {
        if (sr_map[i].spdm_context == spdm_context) {
            return (struct sr_buffer *)sr_map[i].transport;
        }
    }
    if (!allocate) {
        return NULL;
    }
    for (int i = 0; i < SR_MAP_SIZE; i++) {
        if (sr_map[i].spdm_context == NULL) {
            sr_map[i].spdm_context = spdm_context;
            sr_map[i].transport = (struct mock_transport *)&sr_buffers[i];
            sr_buffers[i].acquired = false;
            return &sr_buffers[i];
        }
    }
    return NULL;
}

static libspdm_return_t acquire_sender_buffer(void *spdm_context,
                                              void **msg_buf_ptr)
{
    struct sr_buffer *sr = find_sr(spdm_context, true);

    if (sr == NULL || sr->acquired) {
        return LIBSPDM_STATUS_ACQUIRE_FAIL;
    }
    sr->acquired = true;
    memset(sr->bytes, 0, sizeof(sr->bytes));
    *msg_buf_ptr = sr->bytes;
    return LIBSPDM_STATUS_SUCCESS;
}

static void release_sender_buffer(void *spdm_context, const void *msg_buf_ptr)
{
    struct sr_buffer *sr = find_sr(spdm_context, false);

    ARG_UNUSED(msg_buf_ptr);
    if (sr != NULL) {
        sr->acquired = false;
    }
}

static libspdm_return_t acquire_receiver_buffer(void *spdm_context,
                                                void **msg_buf_ptr)
{
    return acquire_sender_buffer(spdm_context, msg_buf_ptr);
}

static void release_receiver_buffer(void *spdm_context, const void *msg_buf_ptr)
{
    release_sender_buffer(spdm_context, msg_buf_ptr);
}

/* ---------------------------------------------------------------- */
/* Public installer                                                  */
/* ---------------------------------------------------------------- */

#include "library/spdm_transport_mctp_lib.h"

void mock_transport_install(void *spdm_context, struct mock_transport *t)
{
    mock_transport_set(spdm_context, t);

    libspdm_register_device_io_func(spdm_context,
                                    mock_send_message,
                                    mock_receive_message);

    libspdm_register_transport_layer_func(
        spdm_context,
        SPDM_LOOPBACK_BUFFER_SIZE - SPDM_LOOPBACK_TRANSPORT_ADDITIONAL,
        SPDM_LOOPBACK_TRANSPORT_HEADER_SIZE,
        SPDM_LOOPBACK_TRANSPORT_TAIL_SIZE,
        libspdm_transport_mctp_encode_message,
        libspdm_transport_mctp_decode_message);

    libspdm_register_device_buffer_func(spdm_context,
                                        SPDM_LOOPBACK_BUFFER_SIZE,
                                        SPDM_LOOPBACK_BUFFER_SIZE,
                                        acquire_sender_buffer,
                                        release_sender_buffer,
                                        acquire_receiver_buffer,
                                        release_receiver_buffer);
}
