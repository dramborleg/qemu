#include "qemu/osdep.h"
#include "block/bochs.h"
#include "block/parallels.h"
#include "block/qcow.h"
#include "block/qcow2.h"
#include "block/qed.h"
#include "block/vdi.h"
#include "block/vmdk.h"
#include "block/probe.h"
#include "qapi-types.h"
#include "crypto/block.h"

const char *bochs_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score)
{
    const char *format = "bochs";
    const struct bochs_header *bochs = (const void *)buf;
    assert(score);

    if (buf_size < BOCHS_HEADER_SIZE) {
        *score = 0;
        return format;
    }

    if (!strcmp(bochs->magic, BOCHS_HEADER_MAGIC) &&
        !strcmp(bochs->type, REDOLOG_TYPE) &&
        !strcmp(bochs->subtype, GROWING_TYPE) &&
        ((le32_to_cpu(bochs->version) == BOCHS_HEADER_VERSION) ||
        (le32_to_cpu(bochs->version) == BOCHS_HEADER_V1))) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}

const char *cloop_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score)
{
    const char *format = "cloop";
    const char *magic_version_2_0 = "#!/bin/sh\n"
        "#V2.0 Format\n"
        "modprobe cloop file=$0 && mount -r -t iso9660 /dev/cloop $1\n";
    int length = strlen(magic_version_2_0);
    assert(score);

    if (length > buf_size) {
        length = buf_size;
    }
    if (!memcmp(magic_version_2_0, buf, length)) {
        *score = 2;
        return format;
    }

    *score = 0;
    return format;
}

static int block_crypto_probe_generic(QCryptoBlockFormat format,
                                      const uint8_t *buf, int buf_size,
                                      const char *filename)
{
    if (qcrypto_block_has_format(format, buf, buf_size)) {
        return 100;
    } else {
        return 0;
    }
}

const char *block_crypto_probe_luks(const uint8_t *buf, int buf_size,
                                    const char *filename, int *score)
{
    const char *format = "luks";
    assert(score);
    *score = block_crypto_probe_generic(Q_CRYPTO_BLOCK_FORMAT_LUKS,
                                        buf, buf_size, filename);
    return format;
}

const char *dmg_probe(const uint8_t *buf, int buf_size, const char *filename,
                     int *score)
{
    const char *format = "dmg";
    int len;
    assert(score);

    if (!filename) {
        *score = 0;
        return format;
    }

    len = strlen(filename);
    if (len > 4 && !strcmp(filename + len - 4, ".dmg")) {
        *score = 2;
        return format;
    }

    *score = 0;
    return format;
}

const char *parallels_probe(const uint8_t *buf, int buf_size,
                            const char *filename, int *score)
{
    const char *format = "parallels";
    const ParallelsHeader *ph = (const void *)buf;
    assert(score);

    if (buf_size < sizeof(ParallelsHeader)) {
        *score = 0;
        return format;
    }

    if ((!memcmp(ph->magic, PARALLELS_HEADER_MAGIC, 16) ||
        !memcmp(ph->magic, PARALLELS_HEADER_MAGIC2, 16)) &&
        (le32_to_cpu(ph->version) == PARALLELS_HEADER_VERSION)) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}

const char *qcow_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score)
{
    const char *format = "qcow";
    const QCowHeader *cow_header = (const void *)buf;
    assert(score);

    if (buf_size >= sizeof(QCowHeader) &&
        be32_to_cpu(cow_header->magic) == QCOW_MAGIC &&
        be32_to_cpu(cow_header->version) == QCOW_VERSION) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}

const char *qcow2_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score)
{
    const char *format = "qcow2";
    const QCow2Header *cow_header = (const void *)buf;
    assert(score);

    if (buf_size >= sizeof(QCow2Header) &&
        be32_to_cpu(cow_header->magic) == QCOW2_MAGIC &&
        be32_to_cpu(cow_header->version) >= 2) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}

const char *bdrv_qed_probe(const uint8_t *buf, int buf_size,
                           const char *filename, int *score)
{
    const char *format = "qed";
    const QEDHeader *header = (const QEDHeader *)buf;
    assert(score);

    if (buf_size < sizeof(*header)) {
        *score = 0;
        return format;
    }

    if (le32_to_cpu(header->magic) != QED_MAGIC) {
        *score = 0;
        return format;
    }

    *score = 100;
    return format;
}

const char *raw_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score)
{
    const char *format = "raw";
    assert(score);
    /* smallest possible positive score so that raw is used if and only if no
     * other block driver works
     */
    *score = 1;
    return format;
}

const char *vdi_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score)
{
    const char *format = "vdi";
    const VdiHeader *header = (const VdiHeader *)buf;
    assert(score);
    *score = 0;

    logout("\n");

    if (buf_size < sizeof(*header)) {
        /* Header too small, no VDI. */
    } else if (le32_to_cpu(header->signature) == VDI_SIGNATURE) {
        *score = 100;
    }

    if (*score == 0) {
        logout("no vdi image\n");
    } else {
        logout("%s", header->text);
    }

    return format;
}

/*
 * Per the MS VHDX Specification, for every VHDX file:
 *      - The header section is fixed size - 1 MB
 *      - The header section is always the first "object"
 *      - The first 64KB of the header is the File Identifier
 *      - The first uint64 (8 bytes) is the VHDX Signature ("vhdxfile")
 *      - The following 512 bytes constitute a UTF-16 string identifiying the
 *        software that created the file, and is optional and diagnostic only.
 *
 *  Therefore, we probe by looking for the vhdxfile signature "vhdxfile"
 */
const char *vhdx_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score)
{
    const char *format = "vhdx";
    assert(score);

    if (buf_size >= 8 && !memcmp(buf, "vhdxfile", 8)) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}

const char *vmdk_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score)
{
    const char *format = "vmdk";
    uint32_t magic;
    assert(score);

    if (buf_size < 4) {
        *score = 0;
        return format;
    }
    magic = be32_to_cpu(*(uint32_t *)buf);
    if (magic == VMDK3_MAGIC ||
        magic == VMDK4_MAGIC) {
        *score = 100;
        return format;
    } else {
        const char *p = (const char *)buf;
        const char *end = p + buf_size;
        while (p < end) {
            if (*p == '#') {
                /* skip comment line */
                while (p < end && *p != '\n') {
                    p++;
                }
                p++;
                continue;
            }
            if (*p == ' ') {
                while (p < end && *p == ' ') {
                    p++;
                }
                /* skip '\r' if windows line endings used. */
                if (p < end && *p == '\r') {
                    p++;
                }
                /* only accept blank lines before 'version=' line */
                if (p == end || *p != '\n') {
                    *score = 0;
                    return format;
                }
                p++;
                continue;
            }
            if (end - p >= strlen("version=X\n")) {
                if (strncmp("version=1\n", p, strlen("version=1\n")) == 0 ||
                    strncmp("version=2\n", p, strlen("version=2\n")) == 0) {
                    *score = 100;
                    return format;
                }
            }
            if (end - p >= strlen("version=X\r\n")) {
                if (strncmp("version=1\r\n", p, strlen("version=1\r\n")) == 0 ||
                    strncmp("version=2\r\n", p, strlen("version=2\r\n")) == 0) {
                    *score = 100;
                    return format;
                }
            }
            *score = 0;
            return format;
        }
        *score = 0;
        return format;
    }
}

const char *vpc_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score)
{
    const char *format = "vpc";
    assert(score);
    if (buf_size >= 8 && !strncmp((char *)buf, "conectix", 8)) {
        *score = 100;
        return format;
    }

    *score = 0;
    return format;
}
