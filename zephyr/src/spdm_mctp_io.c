/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 *
 *  Implementation of the libspdm <-> libmctp bridge declared in
 *  include/libspdm/zephyr/spdm_mctp_io.h. See that header for the
 *  high-level data flow.
 *
 *  Threading model:
 *
 *    * The libmctp rx callback (rx_message_cb) is invoked from
 *      whichever Zephyr context libmctp's binding pumps from -- a
 *      driver workqueue for the I3C/I2C bindings, the application
 *      thread for the serial binding. We never block in the
 *      callback: we copy the message into one of the rx slots and
 *      hand it off through a k_msgq.
 *    * device_send_message() / device_receive_message() are invoked
 *      from the libspdm state machine, i.e. from the application
 *      thread that called libspdm_init_connection() or
 *      libspdm_responder_dispatch_message().
 *    * acquire/release sender/receiver buffers run on the same
 *      thread as send/receive and are not contended; a simple
 *      in_use bool is enough.
 *
 *  Buffer ownership:
 *
 *    libspdm hands us the same buffer pointer it received from
 *    acquire_sender_buffer() when it calls send_message(). It does
 *    not require the buffer to remain valid after send_message()
 *    returns, so we copy out into a libmctp message on the way down
 *    and let libspdm release the buffer immediately. Likewise on the
 *    way up: device_receive_message() copies an rx slot into the
 *    acquired receiver buffer and the slot is reused for the next
 *    incoming message.
 **/

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <libmctp.h>

#include "library/spdm_common_lib.h"
#include "library/spdm_transport_mctp_lib.h"

#include "libspdm/zephyr/spdm_mctp_io.h"

LOG_MODULE_REGISTER(spdm_mctp_io, CONFIG_LIBSPDM_MCTP_LOG_LEVEL);

/* libspdm doesn't carry a per-context opaque pointer slot, so we keep
 * a small static map between spdm_context and our bridge instance.
 * CONFIG_LIBSPDM_MCTP_MAX_BRIDGES bounds it (2 is enough for a single
 * requester or responder app; raise it if an app drives several).
 */
struct spdm_mctp_io_map_entry {
	const void *spdm_context;
	struct spdm_mctp_io *io;
};

static struct spdm_mctp_io_map_entry
	spdm_mctp_io_map[CONFIG_LIBSPDM_MCTP_MAX_BRIDGES];
static struct k_spinlock spdm_mctp_io_map_lock;

static int spdm_mctp_io_map_insert(const void *spdm_context,
				   struct spdm_mctp_io *io)
{
	k_spinlock_key_t key = k_spin_lock(&spdm_mctp_io_map_lock);

	for (size_t i = 0U; i < ARRAY_SIZE(spdm_mctp_io_map); i++) {
		if (spdm_mctp_io_map[i].spdm_context == NULL) {
			spdm_mctp_io_map[i].spdm_context = spdm_context;
			spdm_mctp_io_map[i].io = io;
			k_spin_unlock(&spdm_mctp_io_map_lock, key);
			return 0;
		}
	}

	k_spin_unlock(&spdm_mctp_io_map_lock, key);
	return -ENOMEM;
}

static struct spdm_mctp_io *spdm_mctp_io_map_lookup(const void *spdm_context)
{
	struct spdm_mctp_io *io = NULL;
	k_spinlock_key_t key = k_spin_lock(&spdm_mctp_io_map_lock);

	for (size_t i = 0U; i < ARRAY_SIZE(spdm_mctp_io_map); i++) {
		if (spdm_mctp_io_map[i].spdm_context == spdm_context) {
			io = spdm_mctp_io_map[i].io;
			break;
		}
	}

	k_spin_unlock(&spdm_mctp_io_map_lock, key);
	return io;
}

/* ---------- libmctp -> bridge: rx callback ---------- */

static void rx_message_cb(uint8_t eid, bool tag_owner, uint8_t msg_tag,
			  void *data, void *msg, size_t len)
{
	struct spdm_mctp_io *io = data;
	struct spdm_mctp_io_rx_slot slot;
	int rc;

	ARG_UNUSED(tag_owner);
	ARG_UNUSED(msg_tag);

	if (io == NULL) {
		LOG_ERR("rx callback fired without an io pointer");
		return;
	}

	if (len == 0U || len > SPDM_MCTP_BUFFER_SIZE) {
		LOG_WRN("dropping rx of unsupported length %zu (max %u)",
			len, (unsigned int)SPDM_MCTP_BUFFER_SIZE);
		return;
	}

	slot.len = len;
	memcpy(slot.buf, msg, len);

	rc = k_msgq_put(&io->rx_msgq, &slot, K_NO_WAIT);
	if (rc != 0) {
		LOG_WRN("rx queue full, dropping msg from eid %u (len %zu)",
			eid, len);
	}
}

/* ---------- libspdm -> bridge: device IO ---------- */

static libspdm_return_t spdm_mctp_device_send_message(void *spdm_context,
						      size_t message_size,
						      const void *message,
						      uint64_t timeout)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);
	int rc;

	ARG_UNUSED(timeout);

	if (io == NULL) {
		LOG_ERR("send_message: no bridge for ctx=%p", spdm_context);
		return LIBSPDM_STATUS_SEND_FAIL;
	}

	if (message_size == 0U || message_size > SPDM_MCTP_BUFFER_SIZE) {
		LOG_ERR("send_message: bad size %zu", message_size);
		return LIBSPDM_STATUS_SEND_FAIL;
	}

	/* libspdm's MCTP transport encode prepends a 1-byte MCTP message
	 * type (0x05 SPDM / 0x06 SECURED-MCTP) to the SPDM payload. That
	 * byte is the first byte of the libmctp message payload, so we
	 * pass it through unchanged.
	 */
	rc = mctp_message_tx(io->mctp_ctx, io->peer_eid, false, 0,
			     (void *)message, message_size);
	if (rc != 0) {
		LOG_ERR("mctp_message_tx to eid %u failed: %d",
			io->peer_eid, rc);
		return LIBSPDM_STATUS_SEND_FAIL;
	}

	return LIBSPDM_STATUS_SUCCESS;
}

static libspdm_return_t spdm_mctp_device_receive_message(void *spdm_context,
							 size_t *message_size,
							 void **message,
							 uint64_t timeout)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);
	struct spdm_mctp_io_rx_slot slot;
	k_timeout_t to;
	int rc;

	if (io == NULL) {
		LOG_ERR("receive_message: no bridge for ctx=%p", spdm_context);
		return LIBSPDM_STATUS_RECEIVE_FAIL;
	}

	if (message == NULL || message_size == NULL || *message == NULL) {
		return LIBSPDM_STATUS_RECEIVE_FAIL;
	}

	/* libspdm passes timeout in microseconds; 0 means "block
	 * forever". Clamp to K_MSEC granularity since k_msgq_get takes
	 * a k_timeout_t.
	 */
	if (timeout == 0U) {
		to = K_FOREVER;
	} else {
		to = K_USEC((int64_t)timeout);
	}

	rc = k_msgq_get(&io->rx_msgq, &slot, to);
	if (rc == -EAGAIN) {
		return LIBSPDM_STATUS_RECEIVE_FAIL;
	}
	if (rc != 0) {
		LOG_ERR("k_msgq_get returned %d", rc);
		return LIBSPDM_STATUS_RECEIVE_FAIL;
	}

	if (slot.len > *message_size) {
		LOG_ERR("rx slot len %zu exceeds caller buffer %zu",
			slot.len, *message_size);
		return LIBSPDM_STATUS_RECEIVE_FAIL;
	}

	memcpy(*message, slot.buf, slot.len);
	*message_size = slot.len;

	return LIBSPDM_STATUS_SUCCESS;
}

/* ---------- libspdm -> bridge: buffer hooks ---------- */

static libspdm_return_t spdm_mctp_acquire_sender_buffer(void *spdm_context,
							void **msg_buf_ptr)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);

	if (io == NULL || msg_buf_ptr == NULL) {
		return LIBSPDM_STATUS_ACQUIRE_FAIL;
	}
	if (io->send_buf_in_use) {
		return LIBSPDM_STATUS_ACQUIRE_FAIL;
	}

	io->send_buf_in_use = true;
	*msg_buf_ptr = io->send_buf;
	return LIBSPDM_STATUS_SUCCESS;
}

static void spdm_mctp_release_sender_buffer(void *spdm_context,
					    const void *msg_buf_ptr)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);

	ARG_UNUSED(msg_buf_ptr);
	if (io == NULL) {
		return;
	}

	io->send_buf_in_use = false;
}

static libspdm_return_t spdm_mctp_acquire_receiver_buffer(void *spdm_context,
							  void **msg_buf_ptr)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);

	if (io == NULL || msg_buf_ptr == NULL) {
		return LIBSPDM_STATUS_ACQUIRE_FAIL;
	}
	if (io->recv_buf_in_use) {
		return LIBSPDM_STATUS_ACQUIRE_FAIL;
	}

	io->recv_buf_in_use = true;
	*msg_buf_ptr = io->recv_buf;
	return LIBSPDM_STATUS_SUCCESS;
}

static void spdm_mctp_release_receiver_buffer(void *spdm_context,
					      const void *msg_buf_ptr)
{
	struct spdm_mctp_io *io = spdm_mctp_io_map_lookup(spdm_context);

	ARG_UNUSED(msg_buf_ptr);
	if (io == NULL) {
		return;
	}

	io->recv_buf_in_use = false;
}

/* ---------- public API ---------- */

int spdm_mctp_io_init(struct spdm_mctp_io *io, struct mctp *mctp_ctx,
		      uint8_t local_eid, uint8_t peer_eid)
{
	if (io == NULL || mctp_ctx == NULL) {
		return -EINVAL;
	}

	memset(io, 0, sizeof(*io));
	io->mctp_ctx = mctp_ctx;
	io->local_eid = local_eid;
	io->peer_eid = peer_eid;

	k_msgq_init(&io->rx_msgq, io->rx_msgq_storage,
		    sizeof(struct spdm_mctp_io_rx_slot),
		    SPDM_MCTP_RX_QUEUE_DEPTH);

	(void)mctp_set_rx_all(mctp_ctx, rx_message_cb, io);
	return 0;
}

int spdm_mctp_io_register(void *spdm_context, struct spdm_mctp_io *io)
{
	int rc;

	if (spdm_context == NULL || io == NULL) {
		return -EINVAL;
	}

	rc = spdm_mctp_io_map_insert(spdm_context, io);
	if (rc != 0) {
		return rc;
	}

	libspdm_register_device_io_func(spdm_context,
					spdm_mctp_device_send_message,
					spdm_mctp_device_receive_message);

	libspdm_register_device_buffer_func(spdm_context,
					    SPDM_MCTP_BUFFER_SIZE,
					    SPDM_MCTP_BUFFER_SIZE,
					    spdm_mctp_acquire_sender_buffer,
					    spdm_mctp_release_sender_buffer,
					    spdm_mctp_acquire_receiver_buffer,
					    spdm_mctp_release_receiver_buffer);
	return 0;
}
