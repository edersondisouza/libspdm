/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

/**
 * @file
 * Embedded-blob registry for libspdm on Zephyr.
 *
 * The libspdm sample device-secret library
 * (os_stub/spdm_device_secret_lib_sample) loads certificate chains and
 * private keys via libspdm_read_input_file(), which on a hosted OS opens
 * the file directly. On Zephyr we instead serve the same lookups out of
 * a small in-RAM table of {path, pointer, length} entries that the
 * application registers at startup.
 *
 * Typical usage:
 *
 *   #include <libspdm/zephyr/secret_blob.h>
 *
 *   static const uint8_t responder_chain[] = {
 *      // generated with `xxd -i bundle_responder.certchain.der`
 *   };
 *
 *   static const struct libspdm_zephyr_secret_blob responder_blobs[] = {
 *       { "ecp256/bundle_responder.certchain.der",
 *         responder_chain, sizeof(responder_chain) },
 *       { "ecp256/end_responder.key.der",
 *         responder_key,   sizeof(responder_key)   },
 *       { NULL, NULL, 0 },
 *   };
 *
 *   libspdm_zephyr_secret_blob_register(responder_blobs);
 */

#ifndef LIBSPDM_ZEPHYR_SECRET_BLOB_H
#define LIBSPDM_ZEPHYR_SECRET_BLOB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct libspdm_zephyr_secret_blob {
    /** Lookup path, e.g. "ecp256/end_responder.key". NULL terminates a table. */
    const char *path;
    /** Pointer to the in-flash/-RAM blob. */
    const uint8_t *data;
    /** Blob length in bytes. */
    size_t length;
};

/**
 * Register a NULL-terminated array of blobs. Multiple tables may be
 * registered (e.g. one per algorithm or per role); lookups walk them in
 * registration order. The pointer must remain valid for the lifetime of
 * the program — typically the blobs live in flash via `static const`.
 *
 * @retval 0          on success
 * @retval -ENOMEM    too many tables registered (raise
 *                    LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES if needed)
 */
int libspdm_zephyr_secret_blob_register(
    const struct libspdm_zephyr_secret_blob *table);

/** Drop all registered tables (mainly for tests). */
void libspdm_zephyr_secret_blob_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSPDM_ZEPHYR_SECRET_BLOB_H */
