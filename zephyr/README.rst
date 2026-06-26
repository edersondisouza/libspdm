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
  and responder, exercised end-to-end through ``GET_VERSION`` /
  ``GET_CAPABILITIES`` / ``NEGOTIATE_ALGORITHMS`` / ``GET_DIGESTS`` /
  ``GET_CERTIFICATE`` / ``CHALLENGE_AUTH`` on ``qemu_x86_64`` (see
  ``samples/spdm_loopback``).
* MCTP transport binding (``libspdm_transport_mctp_*``) — bridged to
  Zephyr's ``libmctp`` via
  ``include/libspdm/zephyr/spdm_mctp_io.h``. Builds cleanly against
  the MCTP-I3C controller and target bindings from ``zephyr/pmci/mctp``
  on ``npcx4m8f_evb`` (see ``samples/spdm_requester_i3c`` and
  ``samples/spdm_responder_i3c``); also drives the in-process
  loopback sample on ``qemu_x86_64``.
* Null crypto backend (compile/link-clean, useful for early bringup).
* mbedTLS crypto backend, built directly from the mbedTLS 3.6.5 LTS
  source tree vendored under ``os_stub/mbedtlslib/`` (Zephyr's own
  mbedTLS module is not used — see *Known limitations* for why).
  End-to-end ECDSA-P256 ``CHALLENGE_AUTH`` with X.509 chain
  verification passes on ``qemu_x86_64``.

Not yet supported:

* PQC (ML-DSA, ML-KEM, SLH-DSA) — the module forces
  ``LIBSPDM_*_SUPPORT=0`` at compile time so the dependent code
  elides. Re-enabling requires a PQC-aware crypto backend.
* Real on-the-wire transports — the libspdm <-> libmctp bridge is
  in place and the I3C samples have been validated on two physical
  ``npcx4m8f_evb`` boards wired together over a common I3C bus
  (handshake reaches ``NEGOTIATE_ALGORITHMS`` with the null crypto
  backend).
* CI on real (non-emulated) targets.

Layout
------

::

  zephyr/
  ├── module.yml                    # tells Zephyr build system this is a module
  ├── Kconfig                       # CONFIG_LIBSPDM_* options
  ├── CMakeLists.txt                # builds the libspdm zephyr_library
  ├── include/libspdm/zephyr/
  │   ├── secret_blob.h             # registry API for embedded keys/certs
  │   └── spdm_mctp_io.h            # libspdm <-> libmctp bridge API
  ├── src/
  │   ├── mbedtls_zephyr_glue.c     # mbedtls_time / mbedtls_ms_time hooks
  │   ├── secret_blob.c             # secret-blob registry + setcert stubs
  │   └── spdm_mctp_io.c            # libspdm <-> libmctp bridge implementation
  └── samples/
      ├── spdm_loopback/            # requester+responder in one app (qemu_x86_64)
      ├── spdm_requester_i3c/       # SPDM requester over MCTP-I3C (npcx4m8f_evb)
      └── spdm_responder_i3c/       # SPDM responder over MCTP-I3C (npcx4m8f_evb)

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

Samples
-------

The module ships three samples (each with its own ``README.rst``):

* ``samples/spdm_loopback`` — requester and responder threads in a
  single Zephyr application, talking through an in-process mock
  MCTP-framed channel. Runs on ``qemu_x86_64``. Prints
  ``*** SPDM handshake (version/caps/algs) PASSED ***`` when the first
  three handshake steps succeed.
* ``samples/spdm_requester_i3c`` — SPDM requester (MCTP EID 20) on
  top of the Zephyr MCTP-I3C controller binding from
  ``zephyr/pmci/mctp``. Targets ``npcx4m8f_evb``.
* ``samples/spdm_responder_i3c`` — SPDM responder (MCTP EID 11) on
  top of the Zephyr MCTP-I3C target binding. Targets
  ``npcx4m8f_evb``. Pair with the requester sample above on a second
  board over a common I3C bus (SDA/SCL/GND) to run the handshake on
  real hardware.

Build any of them with the same ``ZEPHYR_EXTRA_MODULES`` pointing
at the libspdm checkout, e.g.::

  ZEPHYR_EXTRA_MODULES=$(pwd)/libspdm \
      west build -b qemu_x86_64 libspdm/zephyr/samples/spdm_loopback

  ZEPHYR_EXTRA_MODULES=$(pwd)/libspdm \
      west build -b npcx4m8f_evb libspdm/zephyr/samples/spdm_requester_i3c

libspdm <-> libmctp bridge
--------------------------

``include/libspdm/zephyr/spdm_mctp_io.h`` declares a tiny adapter
between libspdm's MCTP transport (DSP0275 framing) and Zephyr's
``libmctp`` (DSP0236 message bus). The adapter is binding-agnostic:
any libmctp binding (I3C controller, I3C target, I2C, serial...)
works without changes to the bridge itself. Typical wiring:

.. code-block:: c

   #include <libmctp.h>
   #include <zephyr/pmci/mctp/mctp_i3c_controller.h>
   #include <libspdm/zephyr/spdm_mctp_io.h>

   MCTP_I3C_CONTROLLER_DT_DEFINE(my_i3c, DT_NODELABEL(mctp_i3c));

   static struct spdm_mctp_io io;

   void app_main(void *spdm_ctx)
   {
       struct mctp *m = mctp_init();

       mctp_register_bus(m, &my_i3c.binding, LOCAL_EID);
       spdm_mctp_io_init(&io, m, LOCAL_EID, PEER_EID);
       spdm_mctp_io_register(spdm_ctx, &io);
       libspdm_register_transport_layer_func(spdm_ctx, ...,
           libspdm_transport_mctp_encode_message,
           libspdm_transport_mctp_decode_message);
   }

Known limitations
-----------------

* **mbedTLS source is vendored, not shared with Zephyr's module.**
  Zephyr v4.4 ships mbedTLS v4.x + TF-PSA-Crypto, which removed the
  legacy ``mbedtls_*_C`` API surface (``mbedtls_hkdf``,
  ``mbedtls_ecdh_*``, ``mbedtls_oid_*`` …) that libspdm's
  ``cryptlib_mbedtls`` is written against. Rather than rewrite the
  cryptlib for the PSA-only API, ``CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y``
  compiles the mbedTLS 3.6.5 LTS source tree already vendored under
  ``os_stub/mbedtlslib/`` directly into the libspdm ``zephyr_library``,
  using libspdm's own pre-curated ``MBEDTLS_CONFIG_FILE``. An
  application that needs to use Zephyr's own mbedTLS module for
  *non-libspdm* code (e.g. its own TLS stack) will therefore link two
  separate copies of mbedTLS — workable, but worth knowing.
* **Wall-clock time is faked.** Zephyr has no battery-backed
  real-time clock by default, but X.509 validity-period checks need a
  wall clock. ``zephyr/src/mbedtls_zephyr_glue.c`` installs a constant
  ``mbedtls_time`` (mid-2025) that sits inside the validity window of
  the DMTF sample certificates. Real deployments must replace this
  with a meaningful time source.
* **No hardware RNG on npcx4m8f.** The Nuvoton ``npcx4`` SoC does not
  expose a hardware DRBG to Zephyr. The I3C samples therefore enable
  ``CONFIG_TEST_RANDOM_GENERATOR=y`` (which selects
  ``TIMER_RANDOM_GENERATOR``), seeding the RNG from
  ``k_cycle_get_32()``. This is **not** suitable for production —
  real deployments must wire a real entropy source (hardware RNG,
  secure-element-backed DRBG, …).
* **Hardware on-wire validation predates the mbedTLS switch.** The
  ``npcx4m8f_evb`` pair was last validated end-to-end through
  ``NEGOTIATE_ALGORITHMS`` with the null backend (commit
  ``737005ed``). After ``903d9578`` / this commit, the I3C samples
  build with ``CONFIG_LIBSPDM_CRYPTO_MBEDTLS=y`` and the same
  authenticated handshake (``GET_DIGESTS`` / ``GET_CERTIFICATE`` /
  ``CHALLENGE_AUTH``) the loopback exercises on ``qemu_x86_64``,
  but re-running this on hardware is pending.
* **No PQC.** All PQC code paths (ML-DSA, ML-KEM, SLH-DSA) are
  compiled out by forcing the relevant ``LIBSPDM_*_SUPPORT`` knobs
  to 0 in the module CMakeLists; the optional GET / SET KEY_PAIR_INFO
  capability is similarly excluded because the sample device-secret
  library's ~1.5 MiB static state would not fit a typical Zephyr
  target. Both restrictions can be lifted by re-defining the
  corresponding compile knobs at the application level.

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
