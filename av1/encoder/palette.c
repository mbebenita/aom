/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>
#include "av1/encoder/palette.h"

static float calc_dist(const float *p1, const float *p2, int dim) {
  float dist = 0;
  int i;
  for (i = 0; i < dim; ++i) {
    const float diff = p1[i] - p2[i];
    dist += diff * diff;
  }
  return dist;
}

void av1_calc_indices(const float *data, const float *centroids,
                      uint8_t *indices, int n, int k, int dim) {
  int i, j;
  for (i = 0; i < n; ++i) {
    float min_dist = calc_dist(data + i * dim, centroids, dim);
    indices[i] = 0;
    for (j = 1; j < k; ++j) {
      const float this_dist =
          calc_dist(data + i * dim, centroids + j * dim, dim);
      if (this_dist < min_dist) {
        min_dist = this_dist;
        indices[i] = j;
      }
    }
  }
}

// Generate a random number in the range [0, 32768).
static unsigned int lcg_rand16(unsigned int *state) {
  *state = *state * 1103515245 + 12345;
  return *state / 65536 % 32768;
}

static void calc_centroids(const float *data, float *centroids,
                           const uint8_t *indices, int n, int k, int dim) {
  int i, j, index;
  int count[PALETTE_MAX_SIZE];
  unsigned int rand_state = (unsigned int)data[0];

  assert(n <= 32768);

  memset(count, 0, sizeof(count[0]) * k);
  memset(centroids, 0, sizeof(centroids[0]) * k * dim);

  for (i = 0; i < n; ++i) {
    index = indices[i];
    assert(index < k);
    ++count[index];
    for (j = 0; j < dim; ++j) {
      centroids[index * dim + j] += data[i * dim + j];
    }
  }

  for (i = 0; i < k; ++i) {
    if (count[i] == 0) {
      memcpy(centroids + i * dim, data + (lcg_rand16(&rand_state) % n) * dim,
             sizeof(centroids[0]) * dim);
    } else {
      const float norm = 1.0f / count[i];
      for (j = 0; j < dim; ++j) centroids[i * dim + j] *= norm;
    }
  }

  // Round to nearest integers.
  for (i = 0; i < k * dim; ++i) {
    centroids[i] = roundf(centroids[i]);
  }
}

static float calc_total_dist(const float *data, const float *centroids,
                             const uint8_t *indices, int n, int k, int dim) {
  float dist = 0;
  int i;
  (void)k;

  for (i = 0; i < n; ++i)
    dist += calc_dist(data + i * dim, centroids + indices[i] * dim, dim);

  return dist;
}

void av1_k_means(const float *data, float *centroids, uint8_t *indices, int n,
                 int k, int dim, int max_itr) {
  int i;
  float this_dist;
  float pre_centroids[2 * PALETTE_MAX_SIZE];
  uint8_t pre_indices[MAX_SB_SQUARE];

  av1_calc_indices(data, centroids, indices, n, k, dim);
  this_dist = calc_total_dist(data, centroids, indices, n, k, dim);

  for (i = 0; i < max_itr; ++i) {
    const float pre_dist = this_dist;
    memcpy(pre_centroids, centroids, sizeof(pre_centroids[0]) * k * dim);
    memcpy(pre_indices, indices, sizeof(pre_indices[0]) * n);

    calc_centroids(data, centroids, indices, n, k, dim);
    av1_calc_indices(data, centroids, indices, n, k, dim);
    this_dist = calc_total_dist(data, centroids, indices, n, k, dim);

    if (this_dist > pre_dist) {
      memcpy(centroids, pre_centroids, sizeof(pre_centroids[0]) * k * dim);
      memcpy(indices, pre_indices, sizeof(pre_indices[0]) * n);
      break;
    }
    if (!memcmp(centroids, pre_centroids, sizeof(pre_centroids[0]) * k * dim))
      break;
  }
}

static int float_comparer(const void *a, const void *b) {
  const float fa = *(const float *)a;
  const float fb = *(const float *)b;
  return (fa > fb) - (fb < fa);
}

int av1_remove_duplicates(float *centroids, int num_centroids) {
  int num_unique;  // number of unique centroids
  int i;
  qsort(centroids, num_centroids, sizeof(*centroids), float_comparer);
  // Remove duplicates.
  num_unique = 1;
  for (i = 1; i < num_centroids; ++i) {
    if (centroids[i] != centroids[i - 1]) {  // found a new unique centroid
      centroids[num_unique++] = centroids[i];
    }
  }
  return num_unique;
}

int av1_count_colors(const uint8_t *src, int stride, int rows, int cols) {
  int n = 0, r, c, i, val_count[256];
  uint8_t val;
  memset(val_count, 0, sizeof(val_count));

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
      val = src[r * stride + c];
      ++val_count[val];
    }
  }

  for (i = 0; i < 256; ++i) {
    if (val_count[i]) {
      ++n;
    }
  }

  return n;
}

#if CONFIG_AOM_HIGHBITDEPTH
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth) {
  int n = 0, r, c, i;
  uint16_t val;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  int val_count[1 << 12];

  assert(bit_depth <= 12);
  memset(val_count, 0, (1 << 12) * sizeof(val_count[0]));
  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
      val = src[r * stride + c];
      ++val_count[val];
    }
  }

  for (i = 0; i < (1 << bit_depth); ++i) {
    if (val_count[i]) {
      ++n;
    }
  }

  return n;
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
