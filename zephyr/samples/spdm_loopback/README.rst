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

It performs the first three steps of the SPDM 1.2 handshake:

* ``GET_VERSION``
* ``GET_CAPABILITIES``
* ``NEGOTIATE_ALGORITHMS``

These three steps fully exercise:

* the libspdm send / receive / buffer-management hooks,
* the MCTP transport framing (``libspdm_transport_mctp_encode_message``
  and ``..._decode_message``), and
* both libspdm state machines on the requester and responder side,

without requiring any cryptographic primitives, so the demo works with
the *null* crypto backend (``CONFIG_LIBSPDM_CRYPTO_NULL=y``).  Extending
the demo to ``GET_DIGESTS / GET_CERTIFICATE / CHALLENGE_AUTH /
GET_MEASUREMENTS`` is a matter of (a) switching to
``CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y`` and (b) registering an embedded
certificate chain and private key via
``libspdm_zephyr_secret_blob_register()`` (see
``include/libspdm/zephyr/secret_blob.h``).

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
   [requester] *** SPDM handshake (version/caps/algs) PASSED ***
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
