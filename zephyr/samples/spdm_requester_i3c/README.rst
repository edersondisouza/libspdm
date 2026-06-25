.. zephyr:code-sample:: spdm_requester_i3c
   :name: libspdm requester over MCTP-I3C
   :relevant-api: spdm_common spdm_requester spdm_transport_mctp

   Run an SPDM requester on top of MCTP over an I3C bus, paired with
   the ``spdm_responder_i3c`` sample on a second board.

Overview
********

This sample exercises the libspdm Zephyr module's MCTP transport
binding against Zephyr's ``libmctp`` and the I3C controller binding
from ``zephyr/pmci/mctp/mctp_i3c_controller``. It drives the first
three steps of the SPDM 1.2 handshake (``GET_VERSION``,
``GET_CAPABILITIES``, ``NEGOTIATE_ALGORITHMS``) against a peer running
the ``spdm_responder_i3c`` sample.

The crypto backend used is ``CONFIG_LIBSPDM_CRYPTO_NULL``, which is
enough for the algorithm negotiation steps. Switching to
``CONFIG_LIBSPDM_CRYPTO_MBEDTLS`` and registering an embedded
certificate chain via ``libspdm_zephyr_secret_blob_register()`` is
needed for ``GET_DIGESTS``, ``GET_CERTIFICATE``, ``CHALLENGE_AUTH``
and ``GET_MEASUREMENTS``.

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

   [00:00:00.123] <inf> spdm_requester_i3c: SPDM requester (libspdm + libmctp + I3C) on npcx4m8f_evb
   [00:00:00.124] <inf> spdm_requester_i3c: local EID=20, peer EID=11
   [00:00:00.125] <inf> spdm_requester_i3c: issuing GET_VERSION
   [00:00:00.130] <inf> spdm_requester_i3c: GET_VERSION ok
   [00:00:00.131] <inf> spdm_requester_i3c: issuing GET_CAPABILITIES + NEGOTIATE_ALGORITHMS
   [00:00:00.137] <inf> spdm_requester_i3c: GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok
   [00:00:00.138] <inf> spdm_requester_i3c: *** SPDM handshake (version/caps/algs) PASSED ***
