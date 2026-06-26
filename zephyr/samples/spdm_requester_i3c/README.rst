.. zephyr:code-sample:: spdm_requester_i3c
   :name: libspdm requester over MCTP-I3C
   :relevant-api: spdm_common spdm_requester spdm_transport_mctp

   Run an SPDM requester on top of MCTP over an I3C bus, paired with
   the ``spdm_responder_i3c`` sample on a second board.

Overview
********

This sample exercises the libspdm Zephyr module's MCTP transport
binding against Zephyr's ``libmctp`` and the I3C controller binding
from ``zephyr/pmci/mctp/mctp_i3c_controller``. It drives the full
authenticated SPDM 1.2 handshake plus a secured-session ping/pong
against a peer running the ``spdm_responder_i3c`` sample:

* ``GET_VERSION`` / ``GET_CAPABILITIES`` / ``NEGOTIATE_ALGORITHMS``
* ``GET_DIGESTS`` / ``GET_CERTIFICATE`` / ``CHALLENGE_AUTH``
  (ECDSA-P256 / SHA-256, X.509 chain verified against an embedded
  DMTF sample root)
* ``KEY_EXCHANGE`` / ``FINISH`` (ECDHE-secp256r1 / AES-256-GCM
  secured session)
* one AEAD-protected app message ("ping" → "pong") followed by
  ``END_SESSION``

The crypto backend used is ``CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y``,
running the vendored mbedTLS 3.6.5 LTS sources. The DMTF sample
ECDSA-P256 certificate chains and keys are embedded via
``libspdm_zephyr_secret_blob_register()`` (see
``samples/common/sample_ecp256.{c,h}`` and
``include/libspdm/zephyr/secret_blob.h``).

Building with ``CONFIG_LIBSPDM_CRYPTO_NULL=y`` instead skips the
authentication and session steps and only exercises the
algorithm-negotiation phase — useful for early bringup on a new
board / wiring.

Wiring
******

Two boards connected over a common I3C bus:

============  =====================  =====================
Pin           Requester board        Responder board
============  =====================  =====================
SDA           J18.20                 J18.20
SCL           J18.19                 J18.19
GND           any GND                any GND
============  =====================  =====================

Use pull-up resistors per the I3C specification (typically 2.2 kOhm
for the controller). Wiring matches the upstream
``samples/subsys/pmci/mctp/i3c_bus_host`` and
``samples/subsys/pmci/mctp/i3c_bus_endpoint`` samples on the same
board, so any wiring guidance there applies.

Building and Running
********************

Flash this sample on the board that will act as the I3C *controller*
(MCTP EID 20) and the companion ``spdm_responder_i3c`` sample on the
board that will act as the I3C *target* (MCTP EID 11). Build order
does not matter, but power the responder first so the controller's
ENTDAA succeeds.

.. code-block:: console

   west build -b npcx4m8f_evb modules/libspdm/zephyr/samples/spdm_requester_i3c
   west flash

Expected output
***************

.. code-block:: console

   [00:00:02.586] <inf> spdm_requester_i3c: issuing GET_VERSION
   [00:00:02.590] <inf> spdm_requester_i3c: GET_VERSION ok
   [00:00:02.590] <inf> spdm_requester_i3c: issuing GET_CAPABILITIES + NEGOTIATE_ALGORITHMS
   [00:00:02.602] <inf> spdm_requester_i3c: GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok
   [00:00:02.609] <inf> spdm_requester_i3c: GET_DIGESTS ok, slot_mask=0x01
   [00:00:05.231] <inf> spdm_requester_i3c: GET_CERTIFICATE ok, chain_size=1390
   [00:00:06.892] <inf> spdm_requester_i3c: CHALLENGE_AUTH ok
   [00:00:06.892] <inf> spdm_requester_i3c: *** SPDM authenticated handshake PASSED ***
   [00:00:06.893] <inf> spdm_requester_i3c: starting secure session + ping/pong
   [00:00:08.5xx] <inf> spdm_requester_i3c: *** SPDM session ping/pong PASSED ***

The crypto-heavy phases (~4-5 s total of ``CHALLENGE_AUTH`` plus
``KEY_EXCHANGE``/``FINISH``) are dominated by software ECDSA-P256
and ECDHE on the Cortex-M4F core — the npcx4m8f has no Zephyr-
visible crypto accelerator.
