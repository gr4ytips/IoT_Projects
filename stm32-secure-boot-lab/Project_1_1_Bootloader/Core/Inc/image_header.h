#pragma once
#include <stdint.h>

/* Boot / app image layout
 * 0x08000000 .. 0x08007FFF : bootloader (32 KB)
 * 0x08008000 .. 0x080081FF : application header (512 B)
 * 0x08008200 ..            : application vector table + image
 */

#define APP_HEADER_ADDR   0x08008000UL
#define APP_VECTOR_ADDR   0x08008200UL
#define FLASH_END_ADDR    0x08080000UL

#define IMAGE_MAGIC       0x474D4942UL  /* "BIMG" */
#define IMAGE_HDR_VER     0x00000002UL
#define IMAGE_HDR_SIZE    0x00000200UL  /* 512 bytes */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t hdr_version;
    uint32_t image_size;      /* bytes from APP_VECTOR_ADDR */
    uint32_t vector_addr;     /* must be APP_VECTOR_ADDR */
    uint32_t app_version;
    uint32_t flags;
    uint8_t  signature[64];   /* raw ECDSA-P256 signature: r||s */
    uint8_t  reserved[424];   /* total header size = 512 bytes */
} image_header_t;

_Static_assert(sizeof(image_header_t) == IMAGE_HDR_SIZE, "image_header_t must be 512 bytes");
