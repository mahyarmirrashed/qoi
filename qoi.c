/**
 * @file qoi.c
 * @author Mahyar Mirrashed (mahyarmirrashed.com)
 * @brief Implementation of the QOI (Quite OK Image) image converter.
 * @version 0.1
 * @date 2022-04-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define QOI_IMPLEMENTATION
#include "qoi.h"

#define MAXIMUM_SIZE  
#define MEMORY_SIZE 64

// according to specification, this byte sequence indicates end of byte stream
static const uint8_t qoi_padding[8] = {0,0,0,0,0,0,0,1};

/* HELPER FUNCTIONS */
static inline uint8_t qoi_hash(const qoi_rgba_t color);
static inline uint32_t qoi_read_32(const uint8_t *const bytes,
  uint32_t *const idx);
static inline void qoi_write_32(uint8_t *const bytes, uint32_t *const idx,
  const uint32_t val);

/**
 * @brief Encodes the data stream into the QOI format.
 * 
 * NOTE: The byte array output from this function must be free()d by the caller.
 * 
 * @param data Data to encode.
 * @param desc Descriptor of image being encoded (width, height, channels, etc.)
 * @param size Output of number of bytes in encoded result.
 * @return void* Encoded byte array output.
 */
void *qoi_encode(const void *const data, const qoi_desc *const desc,
  int *const size) {
  // preconditions
  assert(data != NULL);
  assert(desc != NULL);
  assert(size != NULL);
  assert(desc->width > 0);
  assert(desc->height > 0);
  assert(desc->channels == 3 || desc->channels == 4);
  assert(desc->colorspace <= 1);
  assert(desc->height < QOI_MAXIMUM_PIXELS / desc->width);

  if (data == NULL || desc == NULL || size == NULL || desc->width == 0
  || desc->height == 0 || (desc->channels != 3 && desc->channels != 4)
  || desc->colorspace > 1 || desc->height >= QOI_MAXIMUM_PIXELS / desc->width) {
    errno = EINVAL;
    return NULL;
  }

  uint8_t *bytes;
  uint8_t channels;
  uint8_t i;
  uint32_t idx;
  qoi_rgba_t mem[MEMORY_SIZE];
  uint8_t mem_pos;
  const uint8_t *px;
  qoi_rgba_t px_curr;
  uint32_t px_end;
  uint32_t px_len;
  uint32_t px_pos;
  qoi_rgba_t px_prev;
  uint8_t run;
  uint32_t sz;

  // calculate input image size
  sz = desc->width * desc->height * (desc->channels + 1)
    + QOI_HEADER_SIZE + sizeof(qoi_padding);

  bytes = (uint8_t *)malloc(sz);

  if (!bytes) {
    errno = ENOMEM;
    return NULL;
  }

  idx = 0;

  // write QOI header
  qoi_write_32(bytes, &idx, QOI_MAGIC);
  qoi_write_32(bytes, &idx, desc->width);
  qoi_write_32(bytes, &idx, desc->height);
  bytes[idx++] = desc->channels;
  bytes[idx++] = desc->colorspace;

  // cast data pointer into readable pixels
  px = (const uint8_t *)data;

  // initialize previous and current pixel values
  px_prev = (qoi_rgba_t) { .rgba = { .r = 0, .g = 0, .b = 0, .a = 255 } };
  px_curr = px_prev;

  // calculate ending pixel
  px_len = desc->width * desc->height * desc->channels;
  px_end = px_len - desc->channels;

  // initialize hash table
  memset(mem, 0, sizeof(mem));

  run = 0;
  channels = desc->channels;

  for (px_pos = 0; px_pos < px_len; px_pos += channels) {
    // load in new pixel
    if (channels == 4) {
      px_curr = *(qoi_rgba_t *)(px + px_pos);
    } else {
      px_curr.rgba.r = px[px_pos + 0];
      px_curr.rgba.g = px[px_pos + 1];
      px_curr.rgba.b = px[px_pos + 2];
    }

    // check for run pattern
    if (px_curr.v == px_prev.v) {
      run += 1;

      // wrap around if reached maximum run length or end of pixels
      if (run == QOI_MAXIMUM_RUN || px_pos == px_end) {
        // write as QOI_OP_RUN with bias of -1
        bytes[idx++] = QOI_OP_RUN | (run - 1);
        run = 0;
      }
    } else {
      // complete any existing run
      if (run > 0) {
        // write as QOI_OP_RUN with bias of -1
        bytes[idx++] = QOI_OP_RUN | (run - 1);
        run = 0;
      }

      // calculate pixel index position
      mem_pos = qoi_hash(px_curr);

      // if current pixel matches inside hash table
      if (px_curr.v == mem[mem_pos].v) {
        bytes[idx++] = QOI_OP_INDEX | mem_pos;
      } else {
        mem[mem_pos] = px_curr;

        // check if the alpha value remains unchanged
        if (px_curr.rgba.a == px_prev.rgba.a) {
          int8_t dr = px_curr.rgba.r - px_prev.rgba.r;
          int8_t dg = px_curr.rgba.g - px_prev.rgba.g;
          int8_t db = px_curr.rgba.b - px_prev.rgba.b;

          // write as QOI_OP_DIFF if difference is minimal
          if ((dr >= -2 && dr <= 1)
          && (dg >= -2 && dg <= 1)
          && (db >= -2 && db <= 1)) {
            // write with bias of 2
            bytes[idx++] = QOI_OP_DIFF | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2);
          } else {
            int8_t dr_dg = dr - dg;
            int8_t db_dg = db - dg;

            // write as QOI_OP_LUMA if difference is manageable
            if ((dg >= -32 && dg <= 31)
            && (dr_dg >= -8 && dr_dg <= 7)
            && (db_dg >= -8 && db_dg <= 7)) {
              bytes[idx++] = QOI_OP_LUMA | (dg + 32);
              bytes[idx++] = (dr_dg + 8) << 4 | (db_dg + 8);
            } else {
              // write as QOI_OP_RGB (uncompressed rgb value)
              bytes[idx++] = QOI_OP_RGB;
              bytes[idx++] = px_curr.rgba.r;
              bytes[idx++] = px_curr.rgba.g;
              bytes[idx++] = px_curr.rgba.b;
            }
          }
        } else {
          // write as QOI_OP_RGBA (uncompressed rgba value)
          bytes[idx++] = QOI_OP_RGBA;
          bytes[idx++] = px_curr.rgba.r;
          bytes[idx++] = px_curr.rgba.g;
          bytes[idx++] = px_curr.rgba.b;
          bytes[idx++] = px_curr.rgba.a;
        }
      }
    }

    // assign current pixel as previous pixel for next iteration
    px_prev = px_curr;
  }

  // append padding
  for (i = 0; i < (uint8_t)sizeof(qoi_padding); i += 1) {
    bytes[idx++] = qoi_padding[i];
  }

  // write number of encoded bytes
  *size = idx;

  // return encoded bytes (needs to be free()d later)
  return bytes;
}

/**
 * @brief Decodes the data string from the QOI format.
 * 
 * NOTE: The byte array output from this function must be free()d by the caller.
 * 
 * @param data Data to decode.
 * @param size Number of encoded bytes.
 * @param desc Descriptor of image being decoded (will be populated).
 * @param channels Number of channels to decode into.
 * @return void* Decoded byte array output.
 */
void *qoi_decode(const void *const data, const int size, qoi_desc *const desc,
  int channels) {
  assert(data != NULL);
  assert(desc != NULL);
  assert(channels == 0 || channels == 3 || channels == 4);
  assert(size >= QOI_HEADER_SIZE + (int)sizeof(qoi_padding));

  if (data == NULL || desc == NULL
  || (channels != 0 && channels != 3 && channels != 4)
  || size < QOI_HEADER_SIZE + (int)sizeof(qoi_padding)) {
    errno = EINVAL;
    return NULL;
  }

  const uint8_t *bytes;
  uint8_t byte1;
  uint8_t byte2;
  uint32_t idx;
  uint32_t magic;
  qoi_rgba_t mem[MEMORY_SIZE];
  uint8_t *px;
  uint32_t px_chunks;
  qoi_rgba_t px_curr;
  uint32_t px_len;
  uint32_t px_pos;
  uint8_t run;

  // cast data pointer into readable pixels
  bytes = (const uint8_t *)data;
  idx = 0;

  // read QOI header
  magic = qoi_read_32(bytes, &idx);
  desc->width = qoi_read_32(bytes, &idx);
  desc->height = qoi_read_32(bytes, &idx);
  desc->channels = bytes[idx++];
  desc->colorspace = bytes[idx++];

  if (desc->width == 0 || desc->height == 0 || desc->channels < 3
  || desc->channels > 4 || desc->colorspace > 1 || magic != QOI_MAGIC
  || desc->height >= QOI_MAXIMUM_PIXELS / desc->width) {
    errno = EINVAL;
    return NULL;
  }

  if (channels == 0)
    channels = desc->channels;

  // calculate number of pixels
  px_len = desc->width * desc->height * channels;
  px = (uint8_t *)malloc(px_len);

  if (!px) {
    errno = ENOMEM;
    return NULL;
  }

  // initialize current pixel values
  px_curr = (qoi_rgba_t) { .rgba = { .r = 0, .g = 0, .b = 0, .a = 255 } };

  // initialize hash table
  memset(mem, 0, sizeof(mem));

  // number of pixel chunks
  px_chunks = size - (int)sizeof(qoi_padding);

  run = 0;

  for (px_pos = 0; px_pos < px_len; px_pos += channels) {
    // keep looping until end of run
    if (run > 0) {
      run -= 1;
    } else if (idx < px_chunks) {
      // decode unknown byte sequence
      byte1 = bytes[idx++];

      if (byte1 == QOI_OP_RGBA) {
        px_curr.rgba.r = bytes[idx++];
        px_curr.rgba.g = bytes[idx++];
        px_curr.rgba.b = bytes[idx++];
        px_curr.rgba.a = bytes[idx++];
      } else if (byte1 == QOI_OP_RGB) {
        px_curr.rgba.r = bytes[idx++];
        px_curr.rgba.g = bytes[idx++];
        px_curr.rgba.b = bytes[idx++];
      } else if ((byte1 & QOI_OP_MASK) == QOI_OP_LUMA) {
        byte2 = bytes[idx++];
        int8_t dg = (byte1 & 0x3f) - 32;
        px_curr.rgba.r += dg + ((byte2 >> 4) & 0x0f) - 8;
        px_curr.rgba.g += dg;
        px_curr.rgba.b += dg + (byte2 & 0x0f) - 8;
      } else if ((byte1 & QOI_OP_MASK) == QOI_OP_DIFF) {
        px_curr.rgba.r += ((byte1 >> 4) & 0x03) - 2;
        px_curr.rgba.g += ((byte1 >> 2) & 0x03) - 2;
        px_curr.rgba.b += ((byte1) & 0x03) - 2;
      } else if ((byte1 & QOI_OP_MASK) == QOI_OP_INDEX) {
        px_curr = mem[byte1];
      } else if ((byte1 & QOI_OP_MASK) == QOI_OP_RUN) {
        run = (byte1 & QOI_RUN_MASK);
      }

      mem[qoi_hash(px_curr)] = px_curr;
    }

    // write out decoded pixel
    if (channels == 4) {
      *(qoi_rgba_t *)(px + px_pos) = px_curr;
    } else {
      px[px_pos + 0] = px_curr.rgba.r;
      px[px_pos + 1] = px_curr.rgba.g;
      px[px_pos + 2] = px_curr.rgba.b;
    }
  }

  // return decoded pixels (needs to be free()d later)
  return px;
}

/**
 * @brief Opens the file corresponding to the supplied file name in binary read
 * mode. Allocates memory for the file contents and reads in bytes into
 * allocated memory.
 * 
 * NOTE: It is the responsibility of the caller to free the allocated memory.
 * 
 * @param filename File name/path to read from.
 * @param desc QOI header descriptor (needed for decoding).
 * @param channels Number of channels in the supplied file.
 * @return void* Memory address for the bytes contents of the file read.
 */
void *qoi_read(const char *const filename, const qoi_desc *const desc,
  const int channels) {
  // preconditions
  assert(filename != NULL);
  assert(desc != NULL);
  assert(channels == 0 || channels == 3 || channels == 4);

  if (filename == NULL || desc == NULL
  || (channels != 0 && channels != 3 && channels != 4))
    return NULL;

  int bytes_read;
  void *data;
  FILE *f;
  void *p;
  int rc;
  long sz;

  f = fopen(filename, "rb");

  if (!f) return NULL;

  // seek to end of file and get file size
  rc = fseek(f, 0, SEEK_END);
  assert(rc == 0);
  if (rc != 0) return NULL;

  sz = ftell(f);

  if (sz <= 0) {
    rc = fclose(f);
    assert(rc == 0);
    if (rc != 0) return NULL;

    return NULL;
  }

  // set file pointer back to beginning
  rc = fseek(f, 0, SEEK_SET);
  assert(rc == 0);
  if (rc != 0) return NULL;

  // allocate memory for QOI image contents
  data = malloc(sz);

  if (!data) {
    rc = fclose(f);
    assert(rc == 0);
    if (rc != 0) return NULL;

    return NULL;
  }

  // read QOI image contents
  bytes_read = fread(data, 1, sz, f);

  rc = fclose(f);
  assert(rc == 0);
  if (rc != 0) return NULL;

  // grab decoded data
  p = qoi_decode(data, bytes_read, (qoi_desc *const)desc, channels);

  free(data);

  return p;
}

/**
 * @brief Opens the file corresponding to the supplied file name in binary write
 * mode. After encoding the supplied data into the QOI format, writes the
 * encoded bytes to the file stream.
 * 
 * @param filename File name/path to write to.
 * @param data Block of pixel data to encode and write.
 * @param desc QOI header descriptor (needed for encoding).
 * @return int Number of bytes written to the file.
 */
int qoi_write(const char *const filename, const void *const data,
  const qoi_desc *const desc) {
  // preconditions
  assert(filename != NULL);
  assert(data != NULL);
  assert(desc != NULL);

  if (filename == NULL || data == NULL || desc == NULL) return 0;

  void *p;
  FILE *f;
  int sz;
  int rc;

  f = fopen(filename, "wb");

  if (!f) return 0;

  // grab encoded data
  p = qoi_encode(data, desc, &sz);

  if (!p) {
    rc = fclose(f);
    assert(rc == 0);
    if (rc != 0) return 0;

    return 0;
  }

  // write contents to file stream
  rc = fwrite(p, 1, sz, f);
  assert(rc == sz);
  if (rc != sz) return 0;

  rc = fclose(f);
  assert(rc == 0);
  if (rc != 0) return 0;

  // requisite free on pixels
  free(p);

  return sz;
}

/**
 * @brief Compute the value of the QOI color hash.
 * 
 * @param color Color structure to be hashed.
 */
static inline uint8_t qoi_hash(const qoi_rgba_t color) {
  return (
    color.rgba.r * 3 +
    color.rgba.g * 5 +
    color.rgba.b * 7 +
    color.rgba.a * 11
  ) % MEMORY_SIZE;
}

/**
 * @brief Read a 32-bit unsigned integer value from the byte array.
 * 
 * NOTE: We cannot "hack" the compiler and change the byte/uint8_t array into
 * a uint32_t array because the bytes may not be aligned in memory.
 * 
 * @param bytes Byte array to read from.
 * @param idx Index of where to read from in byte array (will be incremented).
 */
static inline uint32_t qoi_read_32(const uint8_t *const bytes,
  uint32_t *const idx) {
  // preconditions
  assert(bytes != NULL);
  assert(idx != NULL);

  uint32_t a = bytes[(*idx)++];
  uint32_t b = bytes[(*idx)++];
  uint32_t c = bytes[(*idx)++];
  uint32_t d = bytes[(*idx)++];
  return a << 24 | b << 16 | c << 8 | d;
}

/**
 * @brief Write a 32-bit unsigned integer value into the byte array.
 * 
 * NOTE: We cannot "hack" the compiler and change the byte/uint8_t array into
 * a uint32_t array because the bytes may not be aligned in memory.
 * 
 * @param bytes Byte array to write to.
 * @param idx Index of where to write to in byte array (will be incremented).
 * @param val 32-bit integer value to write into byte array.
 */
static inline void qoi_write_32(uint8_t *const bytes, uint32_t *const idx,
  const uint32_t val) {
  // preconditions
  assert(bytes != NULL);
  assert(idx != NULL);

  bytes[(*idx)++] = (0xff000000 & val) >> 24;
  bytes[(*idx)++] = (0x00ff0000 & val) >> 16;
  bytes[(*idx)++] = (0x0000ff00 & val) >> 8;
  bytes[(*idx)++] = (0x000000ff & val);
}
