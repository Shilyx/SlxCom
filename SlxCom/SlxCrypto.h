#ifndef _SLXCRYPTO_H
#define _SLXCRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct _md5ctx {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} md5_ctx;

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} sha1_ctx;

typedef struct _crc32ctx {
    uint32_t crc;
} crc32_ctx;

void md5_init(md5_ctx *ctx);
void md5_update(md5_ctx *ctx, const uint8_t *buff, uint32_t leng);
void md5_final(uint8_t digest[16], md5_ctx *ctx);

void sha1_init(sha1_ctx *ctx);
void sha1_update(sha1_ctx *ctx, const uint8_t *buff, uint32_t leng);
void sha1_final(uint8_t digest[20], sha1_ctx *ctx);

void crc32_init(crc32_ctx *ctx);
void crc32_update(crc32_ctx *ctx, const uint8_t *buff, uint32_t leng);
void crc32_final(uint32_t *digest, crc32_ctx *ctx);

#ifdef __cplusplus
};
#endif

#endif /* _SLXCRYPTO_H */