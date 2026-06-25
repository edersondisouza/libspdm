libspdm Zephyr module
=====================

This directory turns `libspdm <https://github.com/DMTF/libspdm>`_ — the
DMTF reference implementation of SPDM (Security Protocol and Data
Model) — into a Zephyr module. When the module is on Zephyr's
``ZEPHYR_MODULES`` search path (either via ``west.yml`` or by passing
``ZEPHYR_MODULES=<path-to-libspdm>``) the application can enable
``CONFIG_LIBSPDM=y`` and use the libspdm requester / responder APIs
exactly as on any other libspdm-supported OS.

Status
------

The module is in early porting state. What works today:

* SPDM 1.0 / 1.1 / 1.2 message framing and state machines, requester
  and responder, exercised through ``GET_VERSION`` /
  ``GET_CAPABILITIES`` / ``NEGOTIATE_ALGORITHMS`` on ``qemu_x86_64``
  (see ``samples/spdm_loopback``).
* MCTP transport binding (``libspdm_transport_mctp_*``) — used by the
  loopback sample on top of an in-process mock device.
* Null crypto backend (compile/link-clean, useful for early bringup).
* mbedTLS crypto backend wired up against Zephyr's ``mbedtls`` module
  *at the build-system level*. End-to-end exercise of the mbedTLS
  backend on Zephyr — i.e. through ``CHALLENGE_AUTH`` and
  ``GET_MEASUREMENTS`` — has not been validated yet (see
  *Known limitations* below).

Not yet supported:

* PQC (ML-DSA, ML-KEM, SLH-DSA) — the module forces
  ``LIBSPDM_*_SUPPORT=0`` at compile time so the dependent code
  elides. Re-enabling requires a PQC-aware crypto backend.
* Real on-the-wire transports — a Zephyr-libmctp bridge for the MCTP
  transport binding and a two-node demo over MCTP-serial are planned.
* CI on real (non-emulated) targets.

Layout
------

::

  zephyr/
  ├── module.yml               # tells Zephyr build system this is a module
  ├── Kconfig                  # CONFIG_LIBSPDM_* options
  ├── Kconfig.mbedtls          # mbedTLS feature requirements
  ├── CMakeLists.txt           # builds the libspdm zephyr_library
  ├── include/libspdm/zephyr/
  │   └── secret_blob.h        # registry API for embedded keys/certs
  ├── src/
  │   └── secret_blob.c        # secret-blob registry + setcert stubs
  └── samples/
      └── spdm_loopback/       # requester+responder in one app demo

The HAL backends consumed by libspdm itself
(``libspdm_sleep`` / ``libspdm_debug_*`` / ``libspdm_*_pool`` /
``libspdm_get_random_number_64``) live in the upstream tree at
``os_stub/{platform_lib,debuglib,malloclib,rnglib}/*_zephyr.c`` and are
selected via ``CMAKE_SYSTEM_NAME STREQUAL "Zephyr"`` branches in those
directories' ``CMakeLists.txt``.

Configuration
-------------

Pick a role (one or both):

* ``CONFIG_LIBSPDM_REQUESTER`` *(default y)*
* ``CONFIG_LIBSPDM_RESPONDER`` *(default y)*

Pick a crypto backend:

* ``CONFIG_LIBSPDM_CRYPTO_MBEDTLS`` *(default; selects ``CONFIG_MBEDTLS``)*
* ``CONFIG_LIBSPDM_CRYPTO_NULL`` *(no-op crypto, for early bringup)*

Pick the transport bindings you need:

* ``CONFIG_LIBSPDM_TRANSPORT_MCTP`` *(default y)*
* ``CONFIG_LIBSPDM_TRANSPORT_PCI_DOE``
* ``CONFIG_LIBSPDM_TRANSPORT_TCP``
* ``CONFIG_LIBSPDM_TRANSPORT_STORAGE``

See ``Kconfig`` for the full list.

Secret material (private keys and certificate chains)
-----------------------------------------------------

Upstream's ``spdm_device_secret_lib_sample`` reads keys and certs from
the host filesystem via POSIX ``open``/``read``/``close``. That makes
no sense on a typical Zephyr target (no filesystem, no POSIX layer
configured, fixed flash budget), so the Zephyr module replaces the
file-I/O hooks with a small in-RAM registry:

.. code-block:: c

   #include <libspdm/zephyr/secret_blob.h>

   static const struct libspdm_zephyr_secret_blob blobs[] = {
       { "ecp256/end_responder.key.der",   responder_key_der,   responder_key_der_len },
       { "ecp256/bundle_responder.certchain.der",
                                          responder_chain_der, responder_chain_der_len },
   };

   libspdm_zephyr_secret_blob_register(blobs, ARRAY_SIZE(blobs));

When ``libspdm_read_input_file()`` is called by the sample
device-secret library, the matching blob is copied out of the
registry.

Three responder-side hooks that the sample upstream backs with
``set_cert.c`` (``libspdm_is_in_trusted_environment``,
``libspdm_write_certificate_to_nvm``,
``libspdm_get_cert_chain_slot_storage_size``) are provided as
"not supported" stubs on Zephyr because ``set_cert.c`` itself is
excluded — it uses POSIX file I/O to update the on-disk cert chain,
which is out of scope for the initial port. Applications that need a
real ``SET_CERTIFICATE`` flow should override these in their own code.

Sample
------

``samples/spdm_loopback`` brings up a requester and a responder in two
threads of a single Zephyr application, connects them through an
in-process MCTP-framed loopback transport, and runs the first three
steps of the SPDM handshake. Expected output:

.. code-block:: console

   [requester] GET_VERSION ok
   [requester] GET_CAPABILITIES + NEGOTIATE_ALGORITHMS ok
   [requester] *** SPDM handshake (version/caps/algs) PASSED ***

See ``samples/spdm_loopback/README.rst`` for build / run instructions.

Known limitations
-----------------

* **mbedTLS user-config not yet provided.** Zephyr v4.4 ships mbedTLS
  v4.x + TF-PSA-Crypto, where many legacy ``MBEDTLS_*_C`` switches are
  no longer real Kconfigs. ``Kconfig.mbedtls`` declares the libspdm
  feature requirements as plain bools so the module compiles cleanly,
  but the mbedTLS feature set actually enabled is whatever Zephyr's
  default config selects. To do a full SPDM handshake (e.g. ECDSA
  P-256 signature verification in ``CHALLENGE_AUTH``) the application
  most likely has to point ``CONFIG_MBEDTLS_USER_CONFIG_FILE`` at a
  ``mbedtls_user_config_libspdm.h`` that explicitly enables the
  primitives libspdm needs.
* **Loopback only.** The MCTP transport binding has been exercised
  only over an in-process mock channel. A bridge to Zephyr's
  ``libmctp`` module is on the roadmap.
* **No PQC.** As noted above, all PQC code paths are compiled out.

Branch / commit history
-----------------------

The Zephyr port lives on the ``zephyr-port`` branch of the libspdm
repository and was developed in three commits:

#. *os_stub: add Zephyr RTOS HAL backends (Phase 1)* — adds the
   four ``*_zephyr.c`` backends.
#. *zephyr: add Zephyr module skeleton with Kconfig + CMake
   (Phase 2)* — adds ``zephyr/`` proper and the secret-blob
   registry.
#. *zephyr: add loopback sample running on qemu_x86_64
   (Phase 3 milestone)* — adds ``samples/spdm_loopback`` and the
   CMake glue needed to build it.
