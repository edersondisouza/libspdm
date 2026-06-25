/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License.
 *
 *  Bridge between libspdm's MCTP transport (DSP0275 framing) and
 *  Zephyr's libmctp module (DSP0236 message bus). One instance of
 *  struct spdm_mctp_io owns:
 *
 *    * a reference to a fully-initialised libmctp context whose bus
 *      is already registered against a binding (I3C, I2C, serial, ...);
 *    * a default destination EID used by the requester side;
 *    * a sender/receiver buffer pair sized to the largest SPDM
 *      transport message we expect (see
 *      CONFIG_LIBSPDM_MCTP_BUFFER_SIZE);
 *    * a small k_msgq onto which the libmctp RX callback hands off
 *      received messages so device_receive_message() can poll for
 *      them with a timeout.
 *
 *  Typical usage:
 *
 *      static struct spdm_mctp_io io;
 *      spdm_mctp_io_init(&io, mctp, peer_eid);
 *      spdm_mctp_io_register(spdm_context, &io);
 *      libspdm_register_transport_layer_func(
 *              spdm_context,
 *              LIBSPDM_MAX_SPDM_MSG_SIZE,
 *              LIBSPDM_TRANSPORT_HEADER_SIZE,
 *              LIBSPDM_TRANSPORT_TAIL_SIZE,
 *              libspdm_transport_mctp_encode_message,
 *              libspdm_transport_mctp_decode_message);
 *
 *  Both sides (requester and responder) use the same bridge; the only
 *  asymmetry is which EID is "ours" and which is "the peer's", which
 *  the application configures.
 **/

#ifndef LIBSPDM_ZEPHYR_SPDM_MCTP_IO_H_
#define LIBSPDM_ZEPHYR_SPDM_MCTP_IO_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "library/spdm_common_lib.h"

struct mctp;

/* Size of one in-flight MCTP message buffer (sender or receiver). */
#define SPDM_MCTP_BUFFER_SIZE CONFIG_LIBSPDM_MCTP_BUFFER_SIZE

/* Depth of the RX hand-off queue between the libmctp rx callback and
 * device_receive_message().
 */
#define SPDM_MCTP_RX_QUEUE_DEPTH CONFIG_LIBSPDM_MCTP_RX_QUEUE_DEPTH

/* One queue entry. msg points at the message buffer; len is the
 * message length in bytes. The buffer is owned by the io instance
 * (recv_buf below) so the producer hands off ownership of the buffer
 * until the consumer copies it out under device_receive_message().
 */
struct spdm_mctp_io_rx_slot {
	size_t len;
	uint8_t buf[SPDM_MCTP_BUFFER_SIZE];
};

struct spdm_mctp_io {
	struct mctp *mctp_ctx;
	uint8_t peer_eid;
	uint8_t local_eid;

	uint8_t send_buf[SPDM_MCTP_BUFFER_SIZE];
	bool send_buf_in_use;

	struct k_msgq rx_msgq;
	char rx_msgq_storage[SPDM_MCTP_RX_QUEUE_DEPTH *
			     sizeof(struct spdm_mctp_io_rx_slot)];

	uint8_t recv_buf[SPDM_MCTP_BUFFER_SIZE];
	bool recv_buf_in_use;
};

/**
 * @brief Initialise an spdm_mctp_io instance.
 *
 * The libmctp context must already have been created via mctp_init()
 * and a binding registered against it via mctp_register_bus(). This
 * call wires up the rx-all hook so received messages are pushed onto
 * the instance's internal queue. It does NOT take ownership of the
 * mctp context (the application keeps it for the lifetime of the
 * binding).
 *
 * @param io        Storage for the instance, typically a static.
 * @param mctp_ctx  Initialised libmctp instance.
 * @param local_eid The EID this node uses on the bus (registered with
 *                  mctp_register_bus). Used purely for diagnostics.
 * @param peer_eid  The EID of the SPDM peer that send_message will
 *                  target. The requester sets this to the responder's
 *                  EID; the responder usually doesn't need it (the rx
 *                  message carries the source EID), but it is recorded
 *                  here for symmetric send paths.
 *
 * @return 0 on success, negative errno on failure.
 */
int spdm_mctp_io_init(struct spdm_mctp_io *io, struct mctp *mctp_ctx,
		      uint8_t local_eid, uint8_t peer_eid);

/**
 * @brief Register the bridge on a libspdm context.
 *
 * Calls libspdm_register_device_io_func() and
 * libspdm_register_device_buffer_func() with this bridge's hooks.
 * The application is still responsible for calling
 * libspdm_register_transport_layer_func() with the MCTP encode/decode
 * helpers from spdm_transport_mctp_lib.
 *
 * @param spdm_context The libspdm context the bridge should drive.
 * @param io           The bridge instance, previously initialised.
 *
 * @return 0 on success, negative errno if the (spdm_context, io)
 *         association can't be stored.
 */
int spdm_mctp_io_register(void *spdm_context, struct spdm_mctp_io *io);

#endif /* LIBSPDM_ZEPHYR_SPDM_MCTP_IO_H_ */
