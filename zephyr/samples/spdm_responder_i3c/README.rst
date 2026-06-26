.. zephyr:code-sample:: spdm_responder_i3c
   :name: libspdm responder over MCTP-I3C
   :relevant-api: spdm_common spdm_responder spdm_transport_mctp

   Run an SPDM responder on top of MCTP over an I3C bus, paired with
   the ``spdm_requester_i3c`` sample on a second board.

Overview
********

This is the responder counterpart to the ``spdm_requester_i3c``
sample. It registers a single libspdm responder context, binds it to
a Zephyr libmctp instance bound to the I3C target binding from
``zephyr/pmci/mctp/mctp_i3c_target``, installs the DMTF sample
ECDSA-P256 certificate chain into slot 0, registers an
application-message dispatcher that answers a secured "ping" with
"pong", and loops on ``libspdm_responder_dispatch_message`` waiting
for SPDM requests from the peer.

See ``samples/spdm_requester_i3c/README.rst`` for the on-wire flow
the peer drives (full authenticated handshake plus ``KEY_EXCHANGE`` /
``FINISH`` and one AEAD app-message round trip), wiring details, and
build instructions; both samples target ``npcx4m8f_evb`` with
matching I3C overlays.

Building and Running
********************

Flash this sample on the board that will act as the I3C *target*
(MCTP EID 11). Power this board before the controller so its target
PID is available when the controller runs ENTDAA.

.. code-block:: console

   west build -b npcx4m8f_evb modules/libspdm/zephyr/samples/spdm_responder_i3c
   west flash

Expected output
***************

.. code-block:: console

   [00:00:00.123] <inf> spdm_responder_i3c: SPDM responder (libspdm + libmctp + I3C) on npcx4m8f_evb
   [00:00:00.124] <inf> spdm_responder_i3c: local EID=11, peer EID=20
   [00:00:00.125] <inf> spdm_responder_i3c: entering responder dispatch loop
