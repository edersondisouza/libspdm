/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <base.h>
#include "library/memlib.h"
#include "internal/libspdm_device_secret_lib.h"

#include <libspdm/zephyr/secret_blob.h>

#ifndef LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES
#define LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES 8
#endif

static const struct libspdm_zephyr_secret_blob *tables[
    LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES];
static size_t table_count;
static struct k_spinlock blob_lock;

int libspdm_zephyr_secret_blob_register(
    const struct libspdm_zephyr_secret_blob *table)
{
    k_spinlock_key_t key;
    int ret = 0;

    if (table == NULL) {
        return -EINVAL;
    }

    key = k_spin_lock(&blob_lock);
    if (table_count >= LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES) {
        ret = -ENOMEM;
    } else {
        tables[table_count++] = table;
    }
    k_spin_unlock(&blob_lock, key);
    return ret;
}

void libspdm_zephyr_secret_blob_reset(void)
{
    k_spinlock_key_t key = k_spin_lock(&blob_lock);

    for (size_t i = 0; i < LIBSPDM_ZEPHYR_SECRET_BLOB_MAX_TABLES; i++) {
        tables[i] = NULL;
    }
    table_count = 0;
    k_spin_unlock(&blob_lock, key);
}

static const struct libspdm_zephyr_secret_blob *find_blob(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    for (size_t t = 0; t < table_count; t++) {
        const struct libspdm_zephyr_secret_blob *entry = tables[t];

        if (entry == NULL) {
            continue;
        }
        for (; entry->path != NULL; entry++) {
            if (strcmp(entry->path, path) == 0) {
                return entry;
            }
        }
    }
    return NULL;
}

/*
 * libspdm sample device-secret-lib hooks. The libspdm contract is:
 *  - allocate a buffer with malloc()
 *  - copy the file contents in
 *  - return the pointer + size to the caller, which will free() it.
 */
bool libspdm_read_input_file(const char *file_name, void **file_data,
                             size_t *file_size)
{
    const struct libspdm_zephyr_secret_blob *entry;
    void *buf;

    if (file_name == NULL || file_data == NULL || file_size == NULL) {
        return false;
    }

    entry = find_blob(file_name);
    if (entry == NULL) {
        printk("libspdm: no embedded blob for \"%s\"\n", file_name);
        return false;
    }

    buf = malloc(entry->length);
    if (buf == NULL) {
        return false;
    }
    memcpy(buf, entry->data, entry->length);

    *file_data = buf;
    *file_size = entry->length;
    return true;
}

bool libspdm_write_output_file(const char *file_name, const void *file_data,
                               size_t file_size)
{
    /* No persistent store on the Zephyr demo target. CSR/SetCert flows
     * that try to persist will get a false return and surface the
     * limitation to the caller rather than silently dropping data. */
    ARG_UNUSED(file_name);
    ARG_UNUSED(file_data);
    ARG_UNUSED(file_size);
    return false;
}

/*
 * libspdm_dump_hex_str is declared by libspdm_device_secret_lib.h and is
 * normally provided by the host application (spdm-emu's support.c). The
 * sample device-secret library calls it from a few diagnostic paths
 * (PSK derivation, slot-key dump). Provide a minimal printk-based
 * implementation so we don't pull in stdio dependencies.
 */
void libspdm_dump_hex_str(const uint8_t *buffer, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; i++) {
        printk("%02x", buffer[i]);
    }
}

/*
 * Three small responder hooks that the libspdm responder dispatcher
 * links unconditionally (set-cert / get-cert paths). On Zephyr we
 * deliberately have no persistent NVM and no trusted environment, so
 * report that and let libspdm respond with an SPDM error to the peer
 * if these flows are exercised.
 */
bool libspdm_is_in_trusted_environment(void *spdm_context)
{
    ARG_UNUSED(spdm_context);
    return false;
}

bool libspdm_write_certificate_to_nvm(void *spdm_context,
                                      uint8_t slot_id, const void *cert_chain,
                                      size_t cert_chain_size,
                                      uint32_t base_hash_algo,
                                      uint32_t base_asym_algo,
                                      uint32_t pqc_asym_algo,
                                      bool *need_reset, bool *is_busy)
{
    ARG_UNUSED(spdm_context);
    ARG_UNUSED(slot_id);
    ARG_UNUSED(cert_chain);
    ARG_UNUSED(cert_chain_size);
    ARG_UNUSED(base_hash_algo);
    ARG_UNUSED(base_asym_algo);
    ARG_UNUSED(pqc_asym_algo);
    if (need_reset) {
        *need_reset = false;
    }
    if (is_busy) {
        *is_busy = false;
    }
    return false;
}

uint32_t libspdm_get_cert_chain_slot_storage_size(void *spdm_context,
                                                  uint8_t slot_id)
{
    ARG_UNUSED(spdm_context);
    ARG_UNUSED(slot_id);
    return 0;
}
