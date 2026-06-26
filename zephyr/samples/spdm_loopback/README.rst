.. zephyr:code-sample:: spdm_loopback
   :name: libspdm loopback
   :relevant-api: spdm_common spdm_requester spdm_responder spdm_transport_mctp

   In-process SPDM requester ↔ responder demo using libspdm on Zephyr.

Overview
********

This sample exercises the `libspdm <https://github.com/DMTF/libspdm>`_
Zephyr module by running an SPDM requester and an SPDM responder side
by side, in two threads of the same Zephyr application, talking through
a pair of k_sem-synchronised buffers (a "mock transport").

It performs the full SPDM 1.2 authenticated handshake plus a secured
session ping/pong:

* ``GET_VERSION`` / ``GET_CAPABILITIES`` / ``NEGOTIATE_ALGORITHMS``
* ``GET_DIGESTS`` / ``GET_CERTIFICATE`` / ``CHALLENGE_AUTH``
  (ECDSA-P256 device identity, X.509 chain verified against an
  embedded root from the DMTF sample certs)
* ``KEY_EXCHANGE`` / ``FINISH`` (ECDHE-secp256r1 + AES-256-GCM
  secured session)
* a single AEAD-protected application message round trip
  ("ping" → "pong") over that session, before
  ``END_SESSION`` tears it down

This fully exercises:

* the libspdm send / receive / buffer-management hooks,
* the MCTP transport framing (``libspdm_transport_mctp_encode_message``
  and ``..._decode_message``),
* both libspdm state machines on the requester and responder side, and
* the vendored mbedTLS 3.6.5 crypto backend (X.509 parse, ECDSA verify,
  ECDHE, AES-GCM, HKDF).

Building and running
********************

The sample assumes the ``libspdm`` Zephyr module is already discoverable
by your west workspace (either via ``west.yml`` or by passing
``ZEPHYR_MODULES``).

.. code-block:: console

   west build -b qemu_x86_64 modules/libspdm/zephyr/samples/spdm_loopback
   west build -t run

Expected output
***************

.. code-block:: console

   *** Booting Zephyr OS build ... ***

   libspdm Zephyr loopback demo
   ============================
   [responder] starting
   [responder] ready, scratch=26496 bytes
   [requester] starting
   [requester] ready, scratch=26496 bytes
   [requester] GET_VERSION ok
   [requester] GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok
   [requester] GET_DIGESTS ok, slot_mask=0x01
   [requester] GET_CERTIFICATE ok, chain_size=1390
   [requester] CHALLENGE_AUTH ok
   [requester] *** SPDM authenticated handshake PASSED ***
   [requester] encrypted ping/pong ok
   [requester] *** SPDM session ping/pong PASSED ***
   [responder] receive timeout, exiting

Footprint
*********

On ``qemu_x86_64`` with the null crypto backend:

* libspdm static archive: ~5 MiB (mostly thrown away by the linker)
* final ``zephyr.elf``: ~0.9 MiB text
* heap usage at peak: ~64 KiB (two SPDM contexts at ~12 KiB each +
  two scratch buffers at ~26 KiB each + transient mallocs)

Switching to ``CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y`` adds another
~60–80 KiB heap headroom for mbedTLS bignum / ECDSA blinding.
