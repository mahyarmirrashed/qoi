/**
 * @file qoi.c
 * @author Mahyar Mirrashed (mahyarmirrashed.com)
 * @brief Header of the QOI (Quite OK Image) image converter.
 * @version 0.1
 * @date 2022-04-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _QOI_H
#define _QOI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define QOI_HEADER_SIZE     14
#define QOI_MAGIC           0x716f6966  /* "qoif" in hex */
#define QOI_MAXIMUM_PIXELS  (uint32_t)400000000
#define QOI_MAXIMUM_RUN     62

/* QOI operation codes */
#define QOI_OP_INDEX        0x00
#define QOI_OP_DIFF         0x40
#define QOI_OP_LUMA         0x80
#define QOI_OP_RUN          0xc0
#define QOI_OP_RGB          0xfe
#define QOI_OP_RGBA         0xff

#define QOI_OP_MASK         0xc0
#define QOI_RUN_MASK        0x3f

typedef struct QOI_DESC {
  uint32_t  width;      /* image width in pixels (BE) */
  uint32_t  height;     /* image height in pixels (BE) */
  uint8_t   channels;   /* 3 = RGB, 4 = RGBA */
  uint8_t   colorspace; /* 0 = sRGB with linear alpha, 1 = all channels linear */
} qoi_desc;

typedef struct QOI_HEADER {
  char      magic[4];   /* magic bytes "qoif" */
  qoi_desc  descriptor;
} qoi_header;

typedef union QOI_RGBA_T {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  } rgba;
  uint32_t v;
} qoi_rgba_t;

#ifdef QOI_IMPLEMENTATION

enum QOI_COLORSPACE {
  QOI_SRGB,
  QOI_LINEAR
};

void *qoi_encode(const void *const data, const qoi_desc *const desc,
  int *const cnt);
void *qoi_decode(const void *const data, const int size, qoi_desc *const desc,
  const int channels);

#ifndef QOI_NO_STDIO
#include <stdio.h>
void *qoi_read(const char *const filename, const qoi_desc *const desc,
  int channels);
int qoi_write(const char *const filename, const void *const data,
  const qoi_desc *const desc);

#endif  /* QOI_NO_STDIO */
#endif  /* QOI_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif  /* _QOI_H */
