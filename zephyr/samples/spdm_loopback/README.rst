.. zephyr:code-sample:: spdm_loopback
   :name: libspdm loopback
   :relevant-api: spdm_common spdm_requester spdm_responder spdm_transport_mctp

   In-process SPDM requester <-> responder demo using libspdm on Zephyr.

Overview
********

This sample exercises the `libspdm <https://github.com/DMTF/libspdm>`_
Zephyr module by running an SPDM requester and an SPDM responder side
by side, in two threads of the same Zephyr application, talking through
a pair of synchronised buffers.

It performs the basic SPDM negotiation up to authenticating the responder.

Building and running
********************

In a west workspace bootstrapped from ``zephyr/west.yml`` (see
``zephyr/README.rst``), libspdm is found as a module automatically, so
it is enough to run, from the workspace root:

.. code-block:: console

   west build -b qemu_x86_64 libspdm/zephyr/samples/spdm_loopback
   west build -t run

Outside such a workspace, point ``EXTRA_ZEPHYR_MODULES`` at libspdm:

.. code-block:: console

   west build -b qemu_x86_64 <path-to-libspdm>/zephyr/samples/spdm_loopback -- \
       -DEXTRA_ZEPHYR_MODULES=<path-to-libspdm>

Expected output
***************

.. code-block:: console

   ** Booting Zephyr OS build v4.4.0-6198-g603fc3624d1b ***

   libspdm Zephyr loopback demo
   ============================
   [responder] starting
   [responder] ready, scratch=26496 bytes
   [requester] starting
   [requester] ready, scratch=26496 bytes
   [responder] received a request and sent a response
   [requester] GET_VERSION ok
   [responder] received a request and sent a response
   [responder] received a request and sent a response
   [responder] received a request and sent a response
   [requester] GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok
   [responder] received a request and sent a response
   [requester] GET_DIGESTS ok, slot_mask=0x01
   [responder] received a request and sent a response
   [requester] GET_CERTIFICATE ok, chain_size=1390
   [responder] received a request and sent a response
   [requester] CHALLENGE_AUTH ok
   [requester] *** SPDM authenticated handshake PASSED ***
   [responder] received a request and sent a response
   [responder] received a request and sent a response
   [responder] received secured app request "ping", replying "pong"
   [responder] received a request and sent a response
   [requester] sent "ping", received "pong"
   [responder] received a request and sent a response
   [requester] *** SPDM session ping/pong PASSED ***
   [responder] no more requests, exiting

   libspdm Zephyr loopback demo: main exiting
