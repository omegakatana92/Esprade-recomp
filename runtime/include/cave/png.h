/**
 *    Name:        png.h
 *    Description: minimal PNG writer for frame snapshots (RGB, no alpha)
 */
#ifndef ESPRADE_PNG_H
#define ESPRADE_PNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int cave_png_write(const char *path, const uint32_t *pixels, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
