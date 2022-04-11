/**
 * @file main.c
 * @author Mahyar Mirrashed (mahyarmirrashed.com)
 * @brief Interface for the QOI (Quite OK Image) image converter.
 * @version 0.1
 * @date 2022-04-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

int ends_with(const char *const str, const char *const suffix);
int main(const int argc, const char *const *const argv);

/**
 * @brief Main program entry point.
 * 
 * @param argc Number of command-line arguments.
 * @param argv String vector of command-line arguments.
 * @return int Returns EXIT_SUCCESS if conversion succeeds. Otherwise, an
 * appropriate exit code and message is supplied.
 */
int main(const int argc, const char *const *const argv) {
  // preconditions
  assert(argc == 3);
  assert(argv != NULL);
  assert(argv[0] != NULL);
  assert(argv[1] != NULL);
  assert(argv[2] != NULL);
  assert(ends_with(argv[1], ".png") || ends_with(argv[1], ".qoi"));

  // ensure correct number of arguments supplied
  if (argc != 3) {
    puts("Usage: qoi <infile> <outfile>");
    puts("Examples:");
    puts("  qoi input.png output.qoi");
    puts("  qoi input.qoi output.png");
    exit(EINVAL);
  }

  qoi_desc desc;
  void *pixels;
  int width;
  int height;
  int channels;
  int encoded;

  desc = (qoi_desc) {0};

  // read in file
  if (ends_with(argv[1], ".png")) {
    if (!stbi_info(argv[1], &width, &height, &channels)) {
      puts("ERROR: Could not read PNG header.");
      exit(EXIT_FAILURE);
    }

    // force odd encodings to be RGBA
    if (channels != 3) channels = 4;

    pixels = (void *)stbi_load(argv[1], &width, &height, NULL, channels);
  } else if (ends_with(argv[1], ".qoi")) {
    pixels = qoi_read(argv[1], &desc, channels);

    // read out parameter values from descriptor
    width = desc.width;
    height = desc.height;
    channels = desc.channels;
  } else {
    puts("ERROR: Can only convert between .png and .qoi.");
    exit(EINVAL);
  }

  // check if pixels were read
  if (!pixels) {
    puts("ERROR: Could not load/decode.");
    exit(EXIT_FAILURE);
  }

  // write out file
  if (ends_with(argv[2], ".png")) {
    encoded = stbi_write_png(argv[2], width, height, channels, pixels, 0);
  } else if (ends_with(argv[2], ".qoi")) {
    encoded = qoi_write(argv[2], pixels, &(qoi_desc) {
      .width = width,
      .height = height,
      .channels = channels,
      .colorspace = QOI_SRGB
    });
  }

  // check if pixels were written
  if (!encoded) {
    puts("ERROR: Could not write/encode.");
    exit(EXIT_FAILURE);
  }

  // requisite free on pixels
  free(pixels);

  exit(EXIT_SUCCESS);
}

/**
 * @brief Checks whether the input string ends with the supplied suffix string.
 * 
 * @param str Input string.
 * @param suffix Suffix string to check at the end of the input string.
 * @return int Returns 1 if string ends with suffix, otherwise 0. If invalid
 * arguments are provided, -1 is returned.
 */
int ends_with(const char *const str, const char *const suffix) {
  // preconditions
  assert(str != NULL);
  assert(suffix != NULL);
  assert(strlen(str) >= strlen(suffix));

  if (!str || !suffix) return -1;

  const size_t len_str = strlen(str);
  const size_t len_suffix = strlen(suffix);

  if (len_suffix > len_str) return -1;

  return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}
