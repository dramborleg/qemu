#ifndef PROBE_H
#define PROBE_H

typedef const char *BdrvProbeFunc(const uint8_t *buf, int buf_size,
                                  const char *filename, int *score);

const char *bochs_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score);
const char *cloop_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score);
const char *block_crypto_probe_luks(const uint8_t *buf, int buf_size,
                                    const char *filename, int *score);
const char *dmg_probe(const uint8_t *buf, int buf_size, const char *filename,
                     int *score);
const char *parallels_probe(const uint8_t *buf, int buf_size,
                            const char *filename, int *score);
const char *qcow_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score);
const char *qcow2_probe(const uint8_t *buf, int buf_size, const char *filename,
                        int *score);
const char *bdrv_qed_probe(const uint8_t *buf, int buf_size,
                           const char *filename, int *score);
const char *raw_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score);
const char *vdi_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score);
const char *vhdx_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score);
const char *vmdk_probe(const uint8_t *buf, int buf_size, const char *filename,
                       int *score);
const char *vpc_probe(const uint8_t *buf, int buf_size, const char *filename,
                      int *score);

#endif
