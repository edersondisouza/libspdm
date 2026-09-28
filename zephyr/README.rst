libspdm on Zephyr
#################

This directory makes libspdm a `Zephyr module
<https://docs.zephyrproject.org/latest/develop/modules.html>`_ and carries
sample applications for it.

Contents
********

``module.yml``, ``Kconfig``, ``CMakeLists.txt``
   Build libspdm, its vendored mbedTLS and, optionally, its sample
   device-secret library as the ``libspdm`` Zephyr library. Configured
   with ``CONFIG_LIBSPDM_*``.

``include/libspdm_zephyr.h``, ``src/mbedtls_time.c``
   The only Zephyr glue the module itself needs: the mbedTLS platform
   time hooks, and ``libspdm_zephyr_set_wallclock()`` to install the wall
   clock used by X.509 validity checks. Until the application installs
   one, the time reads as the Unix epoch and certificate verification
   fails.

``samples/``
   ``spdm_loopback`` (requester and responder in one node, runs on QEMU),
   ``spdm_requester_mctp`` and ``spdm_responder_mctp`` (MCTP over I3C or
   I2C+GPIO on real boards). ``samples/common/`` holds what they share:
   sample ECDSA-P256 key material, a RAM-backed blob store, the SetCert
   hooks, a fixed or RTC-backed wall clock, and ``sample_mctp_io``, the
   glue between libspdm's device IO hooks and Zephyr's libmctp.


Getting started
***************

A workspace for the samples, with this repository as the west manifest
repository:

.. code-block:: console

   west init -m <libspdm-fork-url> --mf zephyr/west.yml spdm-ws
   cd spdm-ws
   west update
   git -C libspdm submodule update --init os_stub/mbedtlslib/mbedtls
   west build -b qemu_x86_64 libspdm/zephyr/samples/spdm_loopback
   west build -t run

West does not fetch the manifest repository's own submodules, hence the
explicit step; only the mbedTLS one is needed.

Using an existing Zephyr workspace
==================================

Zephyr's manifest imports ``zephyr/submanifests/``, so dropping this file
in as ``zephyr/submanifests/libspdm.yaml`` and running ``west update`` is
enough:

.. code-block:: yaml

   manifest:
     projects:
       - name: libspdm
         url: <libspdm-fork-url>
         revision: main
         path: modules/lib/libspdm
         submodules:
           - path: os_stub/mbedtlslib/mbedtls

Alternatively, point ``EXTRA_ZEPHYR_MODULES`` at a libspdm checkout:

.. code-block:: console

   source <zephyr-workspace>/zephyr/zephyr-env.sh
   west build -b qemu_x86_64 libspdm/zephyr/samples/spdm_loopback \
   -DEXTRA_ZEPHYR_MODULES=<libspdm-fork-path>
   west build -t run

Testing
*******

.. code-block:: console

   $ZEPHYR_BASE/scripts/twister -T libspdm/zephyr/samples \
       -p qemu_x86_64 -p mps2/an521/cpu0 \
       -p npcx4m8f_evb -p frdm_mcxn947/mcxn947/cpu0

The loopback sample runs on QEMU; the MCTP samples are build-only.
