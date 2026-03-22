#include "image_header.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/bignum.h"

/* Replace this with the raw uncompressed SEC1 public key:
 *   0x04 || X(32) || Y(32)
 * Total size must be 65 bytes.
 *
 * The placeholder below is intentionally invalid, so verification will fail
 * until you replace it with your real public key.
 */
/*static const uint8_t boot_pubkey[65] = {
    0x04,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};*/


static const uint8_t boot_pubkey[65] = {
    0x04, 0x95, 0xd9, 0x88, 0xa8, 0xb0, 0xf5, 0xcb,
    0x66, 0xc5, 0x58, 0x80, 0x36, 0x05, 0xc4, 0xf1,
    0x33, 0x21, 0x69, 0x95, 0x66, 0x58, 0xdc, 0xbe,
    0xb7, 0xdb, 0x24, 0x71, 0x6a, 0x8f, 0x75, 0x80,
    0x30, 0xf1, 0xf2, 0x08, 0x4f, 0x4e, 0xfd, 0x2c,
    0xb5, 0x65, 0x87, 0x46, 0x59, 0xa2, 0xb6, 0x15,
    0x86, 0x53, 0xa9, 0xf3, 0xd6, 0xe5, 0x97, 0x8b,
    0x12, 0x2a, 0xe4, 0x0e, 0x09, 0x6f, 0x64, 0xca,
    0x67
};

static int header_is_sane(const image_header_t *hdr)
{
    if (hdr->magic != IMAGE_MAGIC) return 0;
    if (hdr->hdr_version != IMAGE_HDR_VER) return 0;
    if (hdr->vector_addr != APP_VECTOR_ADDR) return 0;
    if (hdr->image_size == 0U) return 0;
    if ((hdr->vector_addr + hdr->image_size) > FLASH_END_ADDR) return 0;
    return 1;
}

int secure_verify_app(const image_header_t *hdr)
{
    int ret = -1;
    uint8_t hash[32];
    mbedtls_sha256_context sha;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi r;
    mbedtls_mpi s;

    if (!header_is_sane(hdr)) {
        return 0;
    }

    mbedtls_sha256_init(&sha);
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha,
                          (const unsigned char *)hdr->vector_addr,
                          hdr->image_size);
    mbedtls_sha256_finish(&sha, hash);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto out;

    ret = mbedtls_ecp_point_read_binary(&grp, &Q, boot_pubkey, sizeof(boot_pubkey));
    if (ret != 0) goto out;

    ret = mbedtls_mpi_read_binary(&r, hdr->signature, 32);
    if (ret != 0) goto out;

    ret = mbedtls_mpi_read_binary(&s, hdr->signature + 32, 32);
    if (ret != 0) goto out;

    ret = mbedtls_ecdsa_verify(&grp, hash, sizeof(hash), &Q, &r, &s);

out:
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);
    mbedtls_sha256_free(&sha);

    return (ret == 0) ? 1 : 0;
}
