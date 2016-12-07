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

#include <assert.h>

#include "./aom_scale_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"

#include "av1/common/blockd.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#if CONFIG_MOTION_VAR
#include "av1/common/onyxc_int.h"
#endif  // CONFIG_MOTION_VAR
#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
#include "av1/common/warped_motion.h"
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

#if CONFIG_EXT_INTER

#define NSMOOTHERS 1
static int get_masked_weight(int m, int smoothness) {
#define SMOOTHER_LEN 32
  static const uint8_t smoothfn[NSMOOTHERS][2 * SMOOTHER_LEN + 1] = { {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  4,  7,  13, 21, 32, 43,
      51, 57, 60, 62, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  } };
  if (m < -SMOOTHER_LEN)
    return 0;
  else if (m > SMOOTHER_LEN)
    return (1 << WEDGE_WEIGHT_BITS);
  else
    return smoothfn[smoothness][m + SMOOTHER_LEN];
}

// [smoother][negative][direction]
DECLARE_ALIGNED(16, static uint8_t,
                wedge_mask_obl[NSMOOTHERS][2][WEDGE_DIRECTIONS]
                              [MASK_MASTER_SIZE * MASK_MASTER_SIZE]);

DECLARE_ALIGNED(16, static uint8_t,
                wedge_signflip_lookup[BLOCK_SIZES][MAX_WEDGE_TYPES]);

// 3 * MAX_WEDGE_SQUARE is an easy to compute and fairly tight upper bound
// on the sum of all mask sizes up to an including MAX_WEDGE_SQUARE.
DECLARE_ALIGNED(16, static uint8_t,
                wedge_mask_buf[2 * MAX_WEDGE_TYPES * 3 * MAX_WEDGE_SQUARE]);

static wedge_masks_type wedge_masks[BLOCK_SIZES][2];

// Some unused wedge codebooks left temporarily to facilitate experiments.
// To be removed when setteld.
static wedge_code_type wedge_codebook_8_hgtw[8] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
};

static wedge_code_type wedge_codebook_8_hltw[8] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static wedge_code_type wedge_codebook_8_heqw[8] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 6 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 6, 4 },
};

#if !USE_LARGE_WEDGE_CODEBOOK
static const wedge_code_type wedge_codebook_16_hgtw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 6 }, { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_hltw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_VERTICAL, 6, 4 },   { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_heqw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 6 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 6, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

const wedge_params_type wedge_params_lookup[BLOCK_SIZES] = {
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[3], 0, wedge_masks[3] },
  { 4, wedge_codebook_16_hgtw, wedge_signflip_lookup[4], 0, wedge_masks[4] },
  { 4, wedge_codebook_16_hltw, wedge_signflip_lookup[5], 0, wedge_masks[5] },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[6], 0, wedge_masks[6] },
  { 4, wedge_codebook_16_hgtw, wedge_signflip_lookup[7], 0, wedge_masks[7] },
  { 4, wedge_codebook_16_hltw, wedge_signflip_lookup[8], 0, wedge_masks[8] },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[9], 0, wedge_masks[9] },
  { 0, wedge_codebook_8_hgtw, wedge_signflip_lookup[10], 0, wedge_masks[10] },
  { 0, wedge_codebook_8_hltw, wedge_signflip_lookup[11], 0, wedge_masks[11] },
  { 0, wedge_codebook_8_heqw, wedge_signflip_lookup[12], 0, wedge_masks[12] },
#if CONFIG_EXT_PARTITION
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
#endif  // CONFIG_EXT_PARTITION
};

#else

static const wedge_code_type wedge_codebook_32_hgtw[32] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 6 }, { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 1 },  { WEDGE_OBLIQUE27, 4, 2 },
  { WEDGE_OBLIQUE27, 4, 3 },  { WEDGE_OBLIQUE27, 4, 5 },
  { WEDGE_OBLIQUE27, 4, 6 },  { WEDGE_OBLIQUE27, 4, 7 },
  { WEDGE_OBLIQUE153, 4, 1 }, { WEDGE_OBLIQUE153, 4, 2 },
  { WEDGE_OBLIQUE153, 4, 3 }, { WEDGE_OBLIQUE153, 4, 5 },
  { WEDGE_OBLIQUE153, 4, 6 }, { WEDGE_OBLIQUE153, 4, 7 },
  { WEDGE_OBLIQUE63, 1, 4 },  { WEDGE_OBLIQUE63, 2, 4 },
  { WEDGE_OBLIQUE63, 3, 4 },  { WEDGE_OBLIQUE63, 5, 4 },
  { WEDGE_OBLIQUE63, 6, 4 },  { WEDGE_OBLIQUE63, 7, 4 },
  { WEDGE_OBLIQUE117, 1, 4 }, { WEDGE_OBLIQUE117, 2, 4 },
  { WEDGE_OBLIQUE117, 3, 4 }, { WEDGE_OBLIQUE117, 5, 4 },
  { WEDGE_OBLIQUE117, 6, 4 }, { WEDGE_OBLIQUE117, 7, 4 },
};

static const wedge_code_type wedge_codebook_32_hltw[32] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_VERTICAL, 6, 4 },   { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 1 },  { WEDGE_OBLIQUE27, 4, 2 },
  { WEDGE_OBLIQUE27, 4, 3 },  { WEDGE_OBLIQUE27, 4, 5 },
  { WEDGE_OBLIQUE27, 4, 6 },  { WEDGE_OBLIQUE27, 4, 7 },
  { WEDGE_OBLIQUE153, 4, 1 }, { WEDGE_OBLIQUE153, 4, 2 },
  { WEDGE_OBLIQUE153, 4, 3 }, { WEDGE_OBLIQUE153, 4, 5 },
  { WEDGE_OBLIQUE153, 4, 6 }, { WEDGE_OBLIQUE153, 4, 7 },
  { WEDGE_OBLIQUE63, 1, 4 },  { WEDGE_OBLIQUE63, 2, 4 },
  { WEDGE_OBLIQUE63, 3, 4 },  { WEDGE_OBLIQUE63, 5, 4 },
  { WEDGE_OBLIQUE63, 6, 4 },  { WEDGE_OBLIQUE63, 7, 4 },
  { WEDGE_OBLIQUE117, 1, 4 }, { WEDGE_OBLIQUE117, 2, 4 },
  { WEDGE_OBLIQUE117, 3, 4 }, { WEDGE_OBLIQUE117, 5, 4 },
  { WEDGE_OBLIQUE117, 6, 4 }, { WEDGE_OBLIQUE117, 7, 4 },
};

static const wedge_code_type wedge_codebook_32_heqw[32] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 6 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 6, 4 },
  { WEDGE_OBLIQUE27, 4, 1 },  { WEDGE_OBLIQUE27, 4, 2 },
  { WEDGE_OBLIQUE27, 4, 3 },  { WEDGE_OBLIQUE27, 4, 5 },
  { WEDGE_OBLIQUE27, 4, 6 },  { WEDGE_OBLIQUE27, 4, 7 },
  { WEDGE_OBLIQUE153, 4, 1 }, { WEDGE_OBLIQUE153, 4, 2 },
  { WEDGE_OBLIQUE153, 4, 3 }, { WEDGE_OBLIQUE153, 4, 5 },
  { WEDGE_OBLIQUE153, 4, 6 }, { WEDGE_OBLIQUE153, 4, 7 },
  { WEDGE_OBLIQUE63, 1, 4 },  { WEDGE_OBLIQUE63, 2, 4 },
  { WEDGE_OBLIQUE63, 3, 4 },  { WEDGE_OBLIQUE63, 5, 4 },
  { WEDGE_OBLIQUE63, 6, 4 },  { WEDGE_OBLIQUE63, 7, 4 },
  { WEDGE_OBLIQUE117, 1, 4 }, { WEDGE_OBLIQUE117, 2, 4 },
  { WEDGE_OBLIQUE117, 3, 4 }, { WEDGE_OBLIQUE117, 5, 4 },
  { WEDGE_OBLIQUE117, 6, 4 }, { WEDGE_OBLIQUE117, 7, 4 },
};

const wedge_params_type wedge_params_lookup[BLOCK_SIZES] = {
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 5, wedge_codebook_32_heqw, wedge_signflip_lookup[3], 0, wedge_masks[3] },
  { 5, wedge_codebook_32_hgtw, wedge_signflip_lookup[4], 0, wedge_masks[4] },
  { 5, wedge_codebook_32_hltw, wedge_signflip_lookup[5], 0, wedge_masks[5] },
  { 5, wedge_codebook_32_heqw, wedge_signflip_lookup[6], 0, wedge_masks[6] },
  { 5, wedge_codebook_32_hgtw, wedge_signflip_lookup[7], 0, wedge_masks[7] },
  { 5, wedge_codebook_32_hltw, wedge_signflip_lookup[8], 0, wedge_masks[8] },
  { 5, wedge_codebook_32_heqw, wedge_signflip_lookup[9], 0, wedge_masks[9] },
  { 0, wedge_codebook_8_hgtw, wedge_signflip_lookup[10], 0, wedge_masks[10] },
  { 0, wedge_codebook_8_hltw, wedge_signflip_lookup[11], 0, wedge_masks[11] },
  { 0, wedge_codebook_8_heqw, wedge_signflip_lookup[12], 0, wedge_masks[12] },
#if CONFIG_EXT_PARTITION
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
  { 0, NULL, NULL, 0, NULL },
#endif  // CONFIG_EXT_PARTITION
};
#endif  // USE_LARGE_WEDGE_CODEBOOK

static const uint8_t *get_wedge_mask_inplace(int wedge_index, int neg,
                                             BLOCK_SIZE sb_type) {
  const uint8_t *master;
  const int bh = 4 << b_height_log2_lookup[sb_type];
  const int bw = 4 << b_width_log2_lookup[sb_type];
  const wedge_code_type *a =
      wedge_params_lookup[sb_type].codebook + wedge_index;
  const int smoother = wedge_params_lookup[sb_type].smoother;
  int woff, hoff;
  const uint8_t wsignflip = wedge_params_lookup[sb_type].signflip[wedge_index];

  assert(wedge_index >= 0 &&
         wedge_index < (1 << get_wedge_bits_lookup(sb_type)));
  woff = (a->x_offset * bw) >> 3;
  hoff = (a->y_offset * bh) >> 3;
  master = wedge_mask_obl[smoother][neg ^ wsignflip][a->direction] +
           MASK_MASTER_STRIDE * (MASK_MASTER_SIZE / 2 - hoff) +
           MASK_MASTER_SIZE / 2 - woff;
  return master;
}

const uint8_t *av1_get_soft_mask(int wedge_index, int wedge_sign,
                                 BLOCK_SIZE sb_type, int offset_x,
                                 int offset_y) {
  const uint8_t *mask =
      get_wedge_mask_inplace(wedge_index, wedge_sign, sb_type);
  if (mask) mask -= (offset_x + offset_y * MASK_MASTER_STRIDE);
  return mask;
}

// get a mask according to the compound type
// TODO(sarahparker) this needs to be extended for other experiments and
// is currently only intended for ext_inter alone
#if CONFIG_EXT_INTER
const uint8_t *av1_get_compound_type_mask(
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type,
    int invert) {
  assert(is_masked_compound_type(comp_data->type));
  switch (comp_data->type) {
    case COMPOUND_WEDGE:
      return av1_get_contiguous_soft_mask(
          comp_data->wedge_index,
          invert ? !comp_data->wedge_sign : comp_data->wedge_sign, sb_type);
    default: assert(0); return NULL;
  }
}
#endif  // CONFIG_EXT_INTER

static void init_wedge_master_masks() {
  int i, j, s;
  const int w = MASK_MASTER_SIZE;
  const int h = MASK_MASTER_SIZE;
  const int stride = MASK_MASTER_STRIDE;
  const int a[2] = { 2, 1 };
  const double asqrt = sqrt(a[0] * a[0] + a[1] * a[1]);
  for (s = 0; s < NSMOOTHERS; s++) {
    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) {
        int x = (2 * j + 1 - w);
        int y = (2 * i + 1 - h);
        int m = (int)rint((a[0] * x + a[1] * y) / asqrt);
        wedge_mask_obl[s][1][WEDGE_OBLIQUE63][i * stride + j] =
            wedge_mask_obl[s][1][WEDGE_OBLIQUE27][j * stride + i] =
                get_masked_weight(m, s);
        wedge_mask_obl[s][1][WEDGE_OBLIQUE117][i * stride + w - 1 - j] =
            wedge_mask_obl[s][1][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] =
                (1 << WEDGE_WEIGHT_BITS) - get_masked_weight(m, s);
        wedge_mask_obl[s][0][WEDGE_OBLIQUE63][i * stride + j] =
            wedge_mask_obl[s][0][WEDGE_OBLIQUE27][j * stride + i] =
                (1 << WEDGE_WEIGHT_BITS) - get_masked_weight(m, s);
        wedge_mask_obl[s][0][WEDGE_OBLIQUE117][i * stride + w - 1 - j] =
            wedge_mask_obl[s][0][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] =
                get_masked_weight(m, s);
        wedge_mask_obl[s][1][WEDGE_VERTICAL][i * stride + j] =
            wedge_mask_obl[s][1][WEDGE_HORIZONTAL][j * stride + i] =
                get_masked_weight(x, s);
        wedge_mask_obl[s][0][WEDGE_VERTICAL][i * stride + j] =
            wedge_mask_obl[s][0][WEDGE_HORIZONTAL][j * stride + i] =
                (1 << WEDGE_WEIGHT_BITS) - get_masked_weight(x, s);
      }
  }
}

// If the signs for the wedges for various blocksizes are
// inconsistent flip the sign flag. Do it only once for every
// wedge codebook.
static void init_wedge_signs() {
  BLOCK_SIZE sb_type;
  memset(wedge_signflip_lookup, 0, sizeof(wedge_signflip_lookup));
  for (sb_type = BLOCK_4X4; sb_type < BLOCK_SIZES; ++sb_type) {
    const int bw = block_size_wide[sb_type];
    const int bh = block_size_high[sb_type];
    const wedge_params_type wedge_params = wedge_params_lookup[sb_type];
    const int wbits = wedge_params.bits;
    const int wtypes = 1 << wbits;
    int i, w;
    if (wbits == 0) continue;
    for (w = 0; w < wtypes; ++w) {
      const uint8_t *mask = get_wedge_mask_inplace(w, 0, sb_type);
      int sum = 0;
      for (i = 0; i < bw; ++i) sum += mask[i];
      for (i = 0; i < bh; ++i) sum += mask[i * MASK_MASTER_STRIDE];
      sum = (sum + (bw + bh) / 2) / (bw + bh);
      wedge_params.signflip[w] = (sum < 32);
    }
  }
}

static void init_wedge_masks() {
  uint8_t *dst = wedge_mask_buf;
  BLOCK_SIZE bsize;
  memset(wedge_masks, 0, sizeof(wedge_masks));
  for (bsize = BLOCK_4X4; bsize < BLOCK_SIZES; ++bsize) {
    const uint8_t *mask;
    const int bw = block_size_wide[bsize];
    const int bh = block_size_high[bsize];
    const wedge_params_type *wedge_params = &wedge_params_lookup[bsize];
    const int wbits = wedge_params->bits;
    const int wtypes = 1 << wbits;
    int w;
    if (wbits == 0) continue;
    for (w = 0; w < wtypes; ++w) {
      mask = get_wedge_mask_inplace(w, 0, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw, NULL, 0, NULL, 0, bw,
                        bh);
      wedge_params->masks[0][w] = dst;
      dst += bw * bh;

      mask = get_wedge_mask_inplace(w, 1, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw, NULL, 0, NULL, 0, bw,
                        bh);
      wedge_params->masks[1][w] = dst;
      dst += bw * bh;
    }
    assert(sizeof(wedge_mask_buf) >= (size_t)(dst - wedge_mask_buf));
  }
}

// Equation of line: f(x, y) = a[0]*(x - a[2]*w/8) + a[1]*(y - a[3]*h/8) = 0
void av1_init_wedge_masks() {
  init_wedge_master_masks();
  init_wedge_signs();
  init_wedge_masks();
}

#if CONFIG_SUPERTX
static void build_masked_compound_wedge_extend(
    uint8_t *dst, int dst_stride, const uint8_t *src0, int src0_stride,
    const uint8_t *src1, int src1_stride, int wedge_index, int wedge_sign,
    BLOCK_SIZE sb_type, int wedge_offset_x, int wedge_offset_y, int h, int w) {
  const int subh = (2 << b_height_log2_lookup[sb_type]) == h;
  const int subw = (2 << b_width_log2_lookup[sb_type]) == w;
  const uint8_t *mask = av1_get_soft_mask(wedge_index, wedge_sign, sb_type,
                                          wedge_offset_x, wedge_offset_y);
  aom_blend_a64_mask(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                     mask, MASK_MASTER_STRIDE, h, w, subh, subw);
}

#if CONFIG_AOM_HIGHBITDEPTH
static void build_masked_compound_wedge_extend_highbd(
    uint8_t *dst_8, int dst_stride, const uint8_t *src0_8, int src0_stride,
    const uint8_t *src1_8, int src1_stride, int wedge_index, int wedge_sign,
    BLOCK_SIZE sb_type, int wedge_offset_x, int wedge_offset_y, int h, int w,
    int bd) {
  const int subh = (2 << b_height_log2_lookup[sb_type]) == h;
  const int subw = (2 << b_width_log2_lookup[sb_type]) == w;
  const uint8_t *mask = av1_get_soft_mask(wedge_index, wedge_sign, sb_type,
                                          wedge_offset_x, wedge_offset_y);
  aom_highbd_blend_a64_mask(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                            src1_stride, mask, MASK_MASTER_STRIDE, h, w, subh,
                            subw, bd);
}
#endif  // CONFIG_AOM_HIGHBITDEPTH
#endif  // CONFIG_SUPERTX

static void build_masked_compound(
    uint8_t *dst, int dst_stride, const uint8_t *src0, int src0_stride,
    const uint8_t *src1, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << b_height_log2_lookup[sb_type]) == h;
  const int subw = (2 << b_width_log2_lookup[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type, 0);
  aom_blend_a64_mask(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                     mask, block_size_wide[sb_type], h, w, subh, subw);
}

#if CONFIG_AOM_HIGHBITDEPTH
static void build_masked_compound_wedge_highbd(
    uint8_t *dst_8, int dst_stride, const uint8_t *src0_8, int src0_stride,
    const uint8_t *src1_8, int src1_stride, int wedge_index, int wedge_sign,
    BLOCK_SIZE sb_type, int h, int w, int bd) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << b_height_log2_lookup[sb_type]) == h;
  const int subw = (2 << b_width_log2_lookup[sb_type]) == w;
  const uint8_t *mask =
      av1_get_contiguous_soft_mask(wedge_index, wedge_sign, sb_type);
  aom_highbd_blend_a64_mask(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                            src1_stride, mask, block_size_wide[sb_type], h, w,
                            subh, subw, bd);
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

void av1_make_masked_inter_predictor(const uint8_t *pre, int pre_stride,
                                     uint8_t *dst, int dst_stride,
                                     const int subpel_x, const int subpel_y,
                                     const struct scale_factors *sf, int w,
                                     int h,
#if CONFIG_DUAL_FILTER
                                     const InterpFilter *interp_filter,
#else
                                     const InterpFilter interp_filter,
#endif
                                     int xs, int ys,
#if CONFIG_SUPERTX
                                     int wedge_offset_x, int wedge_offset_y,
#endif  // CONFIG_SUPERTX
                                     const MACROBLOCKD *xd) {
  const MODE_INFO *mi = xd->mi[0];
  const INTERINTER_COMPOUND_DATA *const comp_data =
      &mi->mbmi.interinter_compound_data;
// The prediction filter types used here should be those for
// the second reference block.
#if CONFIG_DUAL_FILTER
  InterpFilter tmp_ipf[4] = {
    interp_filter[2], interp_filter[3], interp_filter[2], interp_filter[3],
  };
#else
  InterpFilter tmp_ipf = interp_filter;
#endif  // CONFIG_DUAL_FILTER
#if CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_dst_[2 * MAX_SB_SQUARE]);
  uint8_t *tmp_dst = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
                         ? CONVERT_TO_BYTEPTR(tmp_dst_)
                         : tmp_dst_;
  av1_make_inter_predictor(pre, pre_stride, tmp_dst, MAX_SB_SIZE, subpel_x,
                           subpel_y, sf, w, h, 0, tmp_ipf, xs, ys, xd);
#if CONFIG_SUPERTX
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    build_masked_compound_wedge_extend_highbd(
        dst, dst_stride, dst, dst_stride, tmp_dst, MAX_SB_SIZE,
        comp_data->wedge_index, comp_data->wedge_sign, mi->mbmi.sb_type,
        wedge_offset_x, wedge_offset_y, h, w, xd->bd);
  else
    build_masked_compound_wedge_extend(
        dst, dst_stride, dst, dst_stride, tmp_dst, MAX_SB_SIZE,
        comp_data->wedge_index, comp_data->wedge_sign, mi->mbmi.sb_type,
        wedge_offset_x, wedge_offset_y, h, w);
#else
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    build_masked_compound_wedge_highbd(
        dst, dst_stride, dst, dst_stride, tmp_dst, MAX_SB_SIZE,
        comp_data->wedge_index, comp_data->wedge_sign, mi->mbmi.sb_type, h, w,
        xd->bd);
  else
    build_masked_compound(dst, dst_stride, dst, dst_stride, tmp_dst,
                          MAX_SB_SIZE, comp_data, mi->mbmi.sb_type, h, w);
#endif  // CONFIG_SUPERTX
#else   // CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_dst[MAX_SB_SQUARE]);
  av1_make_inter_predictor(pre, pre_stride, tmp_dst, MAX_SB_SIZE, subpel_x,
                           subpel_y, sf, w, h, 0, tmp_ipf, xs, ys, xd);
#if CONFIG_SUPERTX
  build_masked_compound_wedge_extend(dst, dst_stride, dst, dst_stride, tmp_dst,
                                     MAX_SB_SIZE, comp_data->wedge_index,
                                     comp_data->wedge_sign, mi->mbmi.sb_type,
                                     wedge_offset_x, wedge_offset_y, h, w);
#else
  build_masked_compound(dst, dst_stride, dst, dst_stride, tmp_dst, MAX_SB_SIZE,
                        comp_data, mi->mbmi.sb_type, h, w);
#endif  // CONFIG_SUPERTX
#endif  // CONFIG_AOM_HIGHBITDEPTH
}
#endif  // CONFIG_EXT_INTER

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_build_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const MV *src_mv, const struct scale_factors *sf, int w, int h, int ref,
#if CONFIG_DUAL_FILTER
    const InterpFilter *interp_filter,
#else
    const InterpFilter interp_filter,
#endif
    enum mv_precision precision, int x, int y, int bd) {
  const int is_q4 = precision == MV_PRECISION_Q4;
  const MV mv_q4 = { is_q4 ? src_mv->row : src_mv->row * 2,
                     is_q4 ? src_mv->col : src_mv->col * 2 };
  MV32 mv = av1_scale_mv(&mv_q4, x, y, sf);
  const int subpel_x = mv.col & SUBPEL_MASK;
  const int subpel_y = mv.row & SUBPEL_MASK;

  src += (mv.row >> SUBPEL_BITS) * src_stride + (mv.col >> SUBPEL_BITS);

  highbd_inter_predictor(src, src_stride, dst, dst_stride, subpel_x, subpel_y,
                         sf, w, h, ref, interp_filter, sf->x_step_q4,
                         sf->y_step_q4, bd);
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *src_mv,
                               const struct scale_factors *sf, int w, int h,
                               int ref,
#if CONFIG_DUAL_FILTER
                               const InterpFilter *interp_filter,
#else
                               const InterpFilter interp_filter,
#endif
                               enum mv_precision precision, int x, int y) {
  const int is_q4 = precision == MV_PRECISION_Q4;
  const MV mv_q4 = { is_q4 ? src_mv->row : src_mv->row * 2,
                     is_q4 ? src_mv->col : src_mv->col * 2 };
  MV32 mv = av1_scale_mv(&mv_q4, x, y, sf);
  const int subpel_x = mv.col & SUBPEL_MASK;
  const int subpel_y = mv.row & SUBPEL_MASK;

  src += (mv.row >> SUBPEL_BITS) * src_stride + (mv.col >> SUBPEL_BITS);

  inter_predictor(src, src_stride, dst, dst_stride, subpel_x, subpel_y, sf, w,
                  h, ref, interp_filter, sf->x_step_q4, sf->y_step_q4);
}

void build_inter_predictors(MACROBLOCKD *xd, int plane,
#if CONFIG_MOTION_VAR
                            int mi_col_offset, int mi_row_offset,
#endif  // CONFIG_MOTION_VAR
                            int block, int bw, int bh, int x, int y, int w,
                            int h,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                            int wedge_offset_x, int wedge_offset_y,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                            int mi_x, int mi_y) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_MOTION_VAR
  const MODE_INFO *mi = xd->mi[mi_col_offset + xd->mi_stride * mi_row_offset];
  const int build_for_obmc = !(mi_col_offset == 0 && mi_row_offset == 0);
#else
  const MODE_INFO *mi = xd->mi[0];
#endif  // CONFIG_MOTION_VAR
  const int is_compound = has_second_ref(&mi->mbmi);
  int ref;
#if CONFIG_GLOBAL_MOTION
  WarpedMotionParams *gm[2];
  int is_global[2];
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    gm[ref] = &xd->global_motion[mi->mbmi.ref_frame[ref]];
    is_global[ref] =
        (get_y_mode(mi, block) == ZEROMV && gm[ref]->wmtype > TRANSLATION);
  }
  // TODO(sarahparker) remove these once gm works with all experiments
  (void)gm;
  (void)is_global;
#endif  // CONFIG_GLOBAL_MOTION

// TODO(sarahparker) enable the use of DUAL_FILTER in warped motion functions
// in order to allow GLOBAL_MOTION and DUAL_FILTER to work together
#if CONFIG_SUB8X8_MC
#if CONFIG_MOTION_VAR
  if (mi->mbmi.sb_type < BLOCK_8X8 && plane > 0 && !build_for_obmc) {
#else
  if (mi->mbmi.sb_type < BLOCK_8X8 && plane > 0) {
#endif  // CONFIG_MOTION_VAR
    // block size in log2
    const int b4_wl = b_width_log2_lookup[mi->mbmi.sb_type];
    const int b4_hl = b_height_log2_lookup[mi->mbmi.sb_type];
    const int b8_sl = b_width_log2_lookup[BLOCK_8X8];

    // block size
    const int b4_w = 1 << b4_wl;
    const int b4_h = 1 << b4_hl;
    const int b8_s = 1 << b8_sl;
    int idx, idy;

    const int x_base = x;
    const int y_base = y;

    // processing unit size
    const int x_step = w >> (b8_sl - b4_wl);
    const int y_step = h >> (b8_sl - b4_hl);

    for (idy = 0; idy < b8_s; idy += b4_h) {
      for (idx = 0; idx < b8_s; idx += b4_w) {
        const int chr_idx = (idy * 2) + idx;
        for (ref = 0; ref < 1 + is_compound; ++ref) {
          const struct scale_factors *const sf = &xd->block_refs[ref]->sf;
          struct buf_2d *const pre_buf = &pd->pre[ref];
          struct buf_2d *const dst_buf = &pd->dst;
          uint8_t *dst = dst_buf->buf;
          const MV mv = mi->bmi[chr_idx].as_mv[ref].as_mv;
          const MV mv_q4 = clamp_mv_to_umv_border_sb(
              xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);
          uint8_t *pre;
          MV32 scaled_mv;
          int xs, ys, subpel_x, subpel_y;
          const int is_scaled = av1_is_scaled(sf);

          x = x_base + idx * x_step;
          y = y_base + idy * y_step;

          dst += dst_buf->stride * y + x;

          if (is_scaled) {
            pre =
                pre_buf->buf + scaled_buffer_offset(x, y, pre_buf->stride, sf);
            scaled_mv = av1_scale_mv(&mv_q4, mi_x + x, mi_y + y, sf);
            xs = sf->x_step_q4;
            ys = sf->y_step_q4;
          } else {
            pre = pre_buf->buf + y * pre_buf->stride + x;
            scaled_mv.row = mv_q4.row;
            scaled_mv.col = mv_q4.col;
            xs = ys = 16;
          }

          subpel_x = scaled_mv.col & SUBPEL_MASK;
          subpel_y = scaled_mv.row & SUBPEL_MASK;
          pre += (scaled_mv.row >> SUBPEL_BITS) * pre_buf->stride +
                 (scaled_mv.col >> SUBPEL_BITS);

#if CONFIG_EXT_INTER
          if (ref &&
              is_masked_compound_type(mi->mbmi.interinter_compound_data.type))
            av1_make_masked_inter_predictor(
                pre, pre_buf->stride, dst, dst_buf->stride, subpel_x, subpel_y,
                sf, w, h, mi->mbmi.interp_filter, xs, ys,
#if CONFIG_SUPERTX
                wedge_offset_x, wedge_offset_y,
#endif  // CONFIG_SUPERTX
                xd);
          else
#endif  // CONFIG_EXT_INTER
            av1_make_inter_predictor(pre, pre_buf->stride, dst, dst_buf->stride,
                                     subpel_x, subpel_y, sf, x_step, y_step,
                                     ref, mi->mbmi.interp_filter, xs, ys, xd);
        }
      }
    }
    return;
  }
#endif

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const struct scale_factors *const sf = &xd->block_refs[ref]->sf;
    struct buf_2d *const pre_buf = &pd->pre[ref];
    struct buf_2d *const dst_buf = &pd->dst;
    uint8_t *const dst = dst_buf->buf + dst_buf->stride * y + x;
    const MV mv = mi->mbmi.sb_type < BLOCK_8X8
#if CONFIG_MOTION_VAR
                      ? (build_for_obmc ? mi->bmi[block].as_mv[ref].as_mv
                                        : average_split_mvs(pd, mi, ref, block))
#else
                      ? average_split_mvs(pd, mi, ref, block)
#endif  // CONFIG_MOTION_VAR
                      : mi->mbmi.mv[ref].as_mv;

    // TODO(jkoleszar): This clamping is done in the incorrect place for the
    // scaling case. It needs to be done on the scaled MV, not the pre-scaling
    // MV. Note however that it performs the subsampling aware scaling so
    // that the result is always q4.
    // mv_precision precision is MV_PRECISION_Q4.
    const MV mv_q4 = clamp_mv_to_umv_border_sb(
        xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);

    uint8_t *pre;
    MV32 scaled_mv;
    int xs, ys, subpel_x, subpel_y;
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      pre = pre_buf->buf + scaled_buffer_offset(x, y, pre_buf->stride, sf);
      scaled_mv = av1_scale_mv(&mv_q4, mi_x + x, mi_y + y, sf);
      xs = sf->x_step_q4;
      ys = sf->y_step_q4;
    } else {
      pre = pre_buf->buf + (y * pre_buf->stride + x);
      scaled_mv.row = mv_q4.row;
      scaled_mv.col = mv_q4.col;
      xs = ys = 16;
    }

    subpel_x = scaled_mv.col & SUBPEL_MASK;
    subpel_y = scaled_mv.row & SUBPEL_MASK;
    pre += (scaled_mv.row >> SUBPEL_BITS) * pre_buf->stride +
           (scaled_mv.col >> SUBPEL_BITS);

#if CONFIG_EXT_INTER
    if (ref && is_masked_compound_type(mi->mbmi.interinter_compound_data.type))
      av1_make_masked_inter_predictor(pre, pre_buf->stride, dst,
                                      dst_buf->stride, subpel_x, subpel_y, sf,
                                      w, h, mi->mbmi.interp_filter, xs, ys,
#if CONFIG_SUPERTX
                                      wedge_offset_x, wedge_offset_y,
#endif  // CONFIG_SUPERTX
                                      xd);
    else
#else  // CONFIG_EXT_INTER
#if CONFIG_GLOBAL_MOTION
    if (is_global[ref])
      av1_warp_plane(gm[ref],
#if CONFIG_AOM_HIGHBITDEPTH
                     xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                     pre_buf->buf0, pre_buf->width, pre_buf->height,
                     pre_buf->stride, dst, (mi_x >> pd->subsampling_x) + x,
                     (mi_y >> pd->subsampling_y) + y, w, h, dst_buf->stride,
                     pd->subsampling_x, pd->subsampling_y, xs, ys, ref);
    else
#endif  // CONFIG_GLOBAL_MOTION
#endif  // CONFIG_EXT_INTER
      av1_make_inter_predictor(pre, pre_buf->stride, dst, dst_buf->stride,
                               subpel_x, subpel_y, sf, w, h, ref,
                               mi->mbmi.interp_filter, xs, ys, xd);
  }
}

void av1_build_inter_predictor_sub8x8(MACROBLOCKD *xd, int plane, int i, int ir,
                                      int ic, int mi_row, int mi_col) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  MODE_INFO *const mi = xd->mi[0];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mi->mbmi.sb_type, pd);
  const int width = block_size_wide[plane_bsize];
  const int height = block_size_high[plane_bsize];
  uint8_t *const dst = &pd->dst.buf[(ir * pd->dst.stride + ic) << 2];
  int ref;
  const int is_compound = has_second_ref(&mi->mbmi);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const uint8_t *pre =
        &pd->pre[ref].buf[(ir * pd->pre[ref].stride + ic) << 2];
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      av1_highbd_build_inter_predictor(
          pre, pd->pre[ref].stride, dst, pd->dst.stride,
          &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
          ref, mi->mbmi.interp_filter, MV_PRECISION_Q3,
          mi_col * MI_SIZE + 4 * ic, mi_row * MI_SIZE + 4 * ir, xd->bd);
    } else {
      av1_build_inter_predictor(
          pre, pd->pre[ref].stride, dst, pd->dst.stride,
          &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
          ref, mi->mbmi.interp_filter, MV_PRECISION_Q3,
          mi_col * MI_SIZE + 4 * ic, mi_row * MI_SIZE + 4 * ir);
    }
#else
    av1_build_inter_predictor(
        pre, pd->pre[ref].stride, dst, pd->dst.stride,
        &mi->bmi[i].as_mv[ref].as_mv, &xd->block_refs[ref]->sf, width, height,
        ref, mi->mbmi.interp_filter, MV_PRECISION_Q3, mi_col * MI_SIZE + 4 * ic,
        mi_row * MI_SIZE + 4 * ir);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }
}

static void build_inter_predictors_for_planes(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int mi_row, int mi_col,
                                              int plane_from, int plane_to) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = block_size_wide[bsize] >> pd->subsampling_x;
    const int bh = block_size_high[bsize] >> pd->subsampling_y;

    if (xd->mi[0]->mbmi.sb_type < BLOCK_8X8) {
      const PARTITION_TYPE bp = bsize - xd->mi[0]->mbmi.sb_type;
      const int have_vsplit = bp != PARTITION_HORZ;
      const int have_hsplit = bp != PARTITION_VERT;
      const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
      const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);
      const int pw = 8 >> (have_vsplit | pd->subsampling_x);
      const int ph = 8 >> (have_hsplit | pd->subsampling_y);
      int x, y;
      assert(bp != PARTITION_NONE && bp < PARTITION_TYPES);
      assert(bsize == BLOCK_8X8);
      assert(pw * num_4x4_w == bw && ph * num_4x4_h == bh);
      for (y = 0; y < num_4x4_h; ++y)
        for (x = 0; x < num_4x4_w; ++x)
          build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                                 0, 0,
#endif  // CONFIG_MOTION_VAR
                                 y * 2 + x, bw, bh, 4 * x, 4 * y, pw, ph,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 mi_x, mi_y);
    } else {
      build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                             0, 0,
#endif  // CONFIG_MOTION_VAR
                             0, bw, bh, 0, 0, bw, bh,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                             0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                             mi_x, mi_y);
    }
  }
}

void av1_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BUFFER_SET *ctx, BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 0, 0);
#if CONFIG_EXT_INTER
  if (is_interintra_pred(&xd->mi[0]->mbmi)) {
    BUFFER_SET default_ctx = { { xd->plane[0].dst.buf, NULL, NULL },
                               { xd->plane[0].dst.stride, 0, 0 } };
    if (!ctx) ctx = &default_ctx;
    av1_build_interintra_predictors_sby(xd, xd->plane[0].dst.buf,
                                        xd->plane[0].dst.stride, ctx, bsize);
  }
#else
  (void)ctx;
#endif  // CONFIG_EXT_INTER
}

void av1_build_inter_predictors_sbp(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BUFFER_SET *ctx, BLOCK_SIZE bsize,
                                    int plane) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, plane, plane);
#if CONFIG_EXT_INTER
  if (is_interintra_pred(&xd->mi[0]->mbmi)) {
    BUFFER_SET default_ctx = {
      { xd->plane[0].dst.buf, xd->plane[1].dst.buf, xd->plane[2].dst.buf },
      { xd->plane[0].dst.stride, xd->plane[1].dst.stride,
        xd->plane[2].dst.stride }
    };
    if (!ctx) ctx = &default_ctx;
    if (plane == 0) {
      av1_build_interintra_predictors_sby(xd, xd->plane[0].dst.buf,
                                          xd->plane[0].dst.stride, ctx, bsize);
    } else {
      av1_build_interintra_predictors_sbc(xd, xd->plane[plane].dst.buf,
                                          xd->plane[plane].dst.stride, ctx,
                                          plane, bsize);
    }
  }
#else
  (void)ctx;
#endif  // CONFIG_EXT_INTER
}

void av1_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BUFFER_SET *ctx, BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 1,
                                    MAX_MB_PLANE - 1);
#if CONFIG_EXT_INTER
  if (is_interintra_pred(&xd->mi[0]->mbmi)) {
    BUFFER_SET default_ctx = {
      { NULL, xd->plane[1].dst.buf, xd->plane[2].dst.buf },
      { 0, xd->plane[1].dst.stride, xd->plane[2].dst.stride }
    };
    if (!ctx) ctx = &default_ctx;
    av1_build_interintra_predictors_sbuv(
        xd, xd->plane[1].dst.buf, xd->plane[2].dst.buf, xd->plane[1].dst.stride,
        xd->plane[2].dst.stride, ctx, bsize);
  }
#else
  (void)ctx;
#endif  // CONFIG_EXT_INTER
}

void av1_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BUFFER_SET *ctx, BLOCK_SIZE bsize) {
  build_inter_predictors_for_planes(xd, bsize, mi_row, mi_col, 0,
                                    MAX_MB_PLANE - 1);
#if CONFIG_EXT_INTER
  if (is_interintra_pred(&xd->mi[0]->mbmi)) {
    BUFFER_SET default_ctx = {
      { xd->plane[0].dst.buf, xd->plane[1].dst.buf, xd->plane[2].dst.buf },
      { xd->plane[0].dst.stride, xd->plane[1].dst.stride,
        xd->plane[2].dst.stride }
    };
    if (!ctx) ctx = &default_ctx;
    av1_build_interintra_predictors(
        xd, xd->plane[0].dst.buf, xd->plane[1].dst.buf, xd->plane[2].dst.buf,
        xd->plane[0].dst.stride, xd->plane[1].dst.stride,
        xd->plane[2].dst.stride, ctx, bsize);
  }
#else
  (void)ctx;
#endif  // CONFIG_EXT_INTER
}

void av1_setup_dst_planes(struct macroblockd_plane planes[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row,
                          int mi_col) {
  uint8_t *const buffers[MAX_MB_PLANE] = { src->y_buffer, src->u_buffer,
                                           src->v_buffer };
  const int widths[MAX_MB_PLANE] = { src->y_crop_width, src->uv_crop_width,
                                     src->uv_crop_width };
  const int heights[MAX_MB_PLANE] = { src->y_crop_height, src->uv_crop_height,
                                      src->uv_crop_height };
  const int strides[MAX_MB_PLANE] = { src->y_stride, src->uv_stride,
                                      src->uv_stride };
  int i;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblockd_plane *const pd = &planes[i];
    setup_pred_plane(&pd->dst, buffers[i], widths[i], heights[i], strides[i],
                     mi_row, mi_col, NULL, pd->subsampling_x,
                     pd->subsampling_y);
  }
}

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf) {
  if (src != NULL) {
    int i;
    uint8_t *const buffers[MAX_MB_PLANE] = { src->y_buffer, src->u_buffer,
                                             src->v_buffer };
    const int widths[MAX_MB_PLANE] = { src->y_crop_width, src->uv_crop_width,
                                       src->uv_crop_width };
    const int heights[MAX_MB_PLANE] = { src->y_crop_height, src->uv_crop_height,
                                        src->uv_crop_height };
    const int strides[MAX_MB_PLANE] = { src->y_stride, src->uv_stride,
                                        src->uv_stride };
    for (i = 0; i < MAX_MB_PLANE; ++i) {
      struct macroblockd_plane *const pd = &xd->plane[i];
      setup_pred_plane(&pd->pre[idx], buffers[i], widths[i], heights[i],
                       strides[i], mi_row, mi_col, sf, pd->subsampling_x,
                       pd->subsampling_y);
    }
  }
}

#if CONFIG_SUPERTX
static const uint8_t mask_8[8] = { 64, 64, 62, 52, 12, 2, 0, 0 };

static const uint8_t mask_16[16] = { 63, 62, 60, 58, 55, 50, 43, 36,
                                     28, 21, 14, 9,  6,  4,  2,  1 };

static const uint8_t mask_32[32] = { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 63,
                                     61, 57, 52, 45, 36, 28, 19, 12, 7,  3,  1,
                                     0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };

static const uint8_t mask_8_uv[8] = { 64, 64, 62, 52, 12, 2, 0, 0 };

static const uint8_t mask_16_uv[16] = { 64, 64, 64, 64, 61, 53, 45, 36,
                                        28, 19, 11, 3,  0,  0,  0,  0 };

static const uint8_t mask_32_uv[32] = { 64, 64, 64, 64, 64, 64, 64, 64,
                                        64, 64, 64, 64, 60, 54, 46, 36,
                                        28, 18, 10, 4,  0,  0,  0,  0,
                                        0,  0,  0,  0,  0,  0,  0,  0 };

static const uint8_t *get_supertx_mask(int length, int plane) {
  switch (length) {
    case 8: return plane ? mask_8_uv : mask_8;
    case 16: return plane ? mask_16_uv : mask_16;
    case 32: return plane ? mask_32_uv : mask_32;
    default: assert(0);
  }
  return NULL;
}

void av1_build_masked_inter_predictor_complex(
    MACROBLOCKD *xd, uint8_t *dst, int dst_stride, const uint8_t *pre,
    int pre_stride, int mi_row, int mi_col, int mi_row_ori, int mi_col_ori,
    BLOCK_SIZE bsize, BLOCK_SIZE top_bsize, PARTITION_TYPE partition,
    int plane) {
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const int ssx = pd->subsampling_x;
  const int ssy = pd->subsampling_y;
  const int top_w = (4 << b_width_log2_lookup[top_bsize]) >> ssx;
  const int top_h = (4 << b_height_log2_lookup[top_bsize]) >> ssy;
  const int w = (4 << b_width_log2_lookup[bsize]) >> ssx;
  const int h = (4 << b_height_log2_lookup[bsize]) >> ssy;
  const int w_offset = ((mi_col - mi_col_ori) * MI_SIZE) >> ssx;
  const int h_offset = ((mi_row - mi_row_ori) * MI_SIZE) >> ssy;

  int w_remain, h_remain;

#if CONFIG_AOM_HIGHBITDEPTH
  const int is_hdb = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  assert(bsize <= BLOCK_32X32);
  assert(IMPLIES(plane == 0, ssx == 0));
  assert(IMPLIES(plane == 0, ssy == 0));

  switch (partition) {
    case PARTITION_HORZ: {
      const uint8_t *const mask = get_supertx_mask(h, ssy);

      w_remain = top_w;
      h_remain = top_h - h_offset - h;
      dst += h_offset * dst_stride;
      pre += h_offset * pre_stride;

#if CONFIG_AOM_HIGHBITDEPTH
      if (is_hdb)
        aom_highbd_blend_a64_vmask(dst, dst_stride, dst, dst_stride, pre,
                                   pre_stride, mask, h, top_w, xd->bd);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        aom_blend_a64_vmask(dst, dst_stride, dst, dst_stride, pre, pre_stride,
                            mask, h, top_w);

      dst += h * dst_stride;
      pre += h * pre_stride;
      break;
    }
    case PARTITION_VERT: {
      const uint8_t *const mask = get_supertx_mask(w, ssx);

      w_remain = top_w - w_offset - w;
      h_remain = top_h;
      dst += w_offset;
      pre += w_offset;

#if CONFIG_AOM_HIGHBITDEPTH
      if (is_hdb)
        aom_highbd_blend_a64_hmask(dst, dst_stride, dst, dst_stride, pre,
                                   pre_stride, mask, top_h, w, xd->bd);
      else
#endif  // CONFIG_AOM_HIGHBITDEPTH
        aom_blend_a64_hmask(dst, dst_stride, dst, dst_stride, pre, pre_stride,
                            mask, top_h, w);

      dst += w;
      pre += w;
      break;
    }
    default: {
      assert(0);
      return;
    }
  }

  if (w_remain == 0 || h_remain == 0) {
    return;
  }

#if CONFIG_AOM_HIGHBITDEPTH
  if (is_hdb) {
    dst = (uint8_t *)CONVERT_TO_SHORTPTR(dst);
    pre = (const uint8_t *)CONVERT_TO_SHORTPTR(pre);
    dst_stride *= 2;
    pre_stride *= 2;
    w_remain *= 2;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH

  do {
    memcpy(dst, pre, w_remain * sizeof(uint8_t));
    dst += dst_stride;
    pre += pre_stride;
  } while (--h_remain);
}

void av1_build_inter_predictors_sb_sub8x8_extend(MACROBLOCKD *xd,
#if CONFIG_EXT_INTER
                                                 int mi_row_ori, int mi_col_ori,
#endif  // CONFIG_EXT_INTER
                                                 int mi_row, int mi_col,
                                                 BLOCK_SIZE bsize, int block) {
  // Prediction function used in supertx:
  // Use the mv at current block (which is less than 8x8)
  // to get prediction of a block located at (mi_row, mi_col) at size of bsize
  // bsize can be larger than 8x8.
  // block (0-3): the sub8x8 location of current block
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
#if CONFIG_EXT_INTER
  const int wedge_offset_x = (mi_col_ori - mi_col) * MI_SIZE;
  const int wedge_offset_y = (mi_row_ori - mi_row) * MI_SIZE;
#endif  // CONFIG_EXT_INTER

  // For sub8x8 uv:
  // Skip uv prediction in supertx except the first block (block = 0)
  int max_plane = block ? 1 : MAX_MB_PLANE;

  for (plane = 0; plane < max_plane; plane++) {
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, &xd->plane[plane]);
    const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
    const int bw = 4 * num_4x4_w;
    const int bh = 4 * num_4x4_h;

    build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                           0, 0,
#endif  // CONFIG_MOTION_VAR
                           block, bw, bh, 0, 0, bw, bh,
#if CONFIG_EXT_INTER
                           wedge_offset_x, wedge_offset_y,
#endif  // CONFIG_EXT_INTER
                           mi_x, mi_y);
  }
#if CONFIG_EXT_INTER
  BUFFER_SET ctx = { { xd->plane[0].dst.buf, xd->plane[1].dst.buf,
                       xd->plane[2].dst.buf },
                     { xd->plane[0].dst.stride, xd->plane[1].dst.stride,
                       xd->plane[2].dst.stride } };
  if (is_interintra_pred(&xd->mi[0]->mbmi))
    av1_build_interintra_predictors(
        xd, xd->plane[0].dst.buf, xd->plane[1].dst.buf, xd->plane[2].dst.buf,
        xd->plane[0].dst.stride, xd->plane[1].dst.stride,
        xd->plane[2].dst.stride, &ctx, bsize);
#endif  // CONFIG_EXT_INTER
}

void av1_build_inter_predictors_sb_extend(MACROBLOCKD *xd,
#if CONFIG_EXT_INTER
                                          int mi_row_ori, int mi_col_ori,
#endif  // CONFIG_EXT_INTER
                                          int mi_row, int mi_col,
                                          BLOCK_SIZE bsize) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
#if CONFIG_EXT_INTER
  const int wedge_offset_x = (mi_col_ori - mi_col) * MI_SIZE;
  const int wedge_offset_y = (mi_row_ori - mi_row) * MI_SIZE;
#endif  // CONFIG_EXT_INTER
  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, &xd->plane[plane]);
    const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];

    if (xd->mi[0]->mbmi.sb_type < BLOCK_8X8) {
      int x, y;
      assert(bsize == BLOCK_8X8);
      for (y = 0; y < num_4x4_h; ++y)
        for (x = 0; x < num_4x4_w; ++x)
          build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                                 0, 0,
#endif  // CONFIG_MOTION_VAR
                                 y * 2 + x, bw, bh, 4 * x, 4 * y, 4, 4,
#if CONFIG_EXT_INTER
                                 wedge_offset_x, wedge_offset_y,
#endif  // CONFIG_EXT_INTER
                                 mi_x, mi_y);
    } else {
      build_inter_predictors(xd, plane,
#if CONFIG_MOTION_VAR
                             0, 0,
#endif  // CONFIG_MOTION_VAR
                             0, bw, bh, 0, 0, bw, bh,
#if CONFIG_EXT_INTER
                             wedge_offset_x, wedge_offset_y,
#endif  // CONFIG_EXT_INTER
                             mi_x, mi_y);
    }
  }
}
#endif  // CONFIG_SUPERTX

#if CONFIG_MOTION_VAR
// obmc_mask_N[overlap_position]
static const uint8_t obmc_mask_1[1] = { 55 };

static const uint8_t obmc_mask_2[2] = { 45, 62 };

static const uint8_t obmc_mask_4[4] = { 39, 50, 59, 64 };

static const uint8_t obmc_mask_8[8] = { 36, 42, 48, 53, 57, 61, 63, 64 };

static const uint8_t obmc_mask_16[16] = { 34, 37, 40, 43, 46, 49, 52, 54,
                                          56, 58, 60, 61, 63, 64, 64, 64 };

static const uint8_t obmc_mask_32[32] = { 33, 35, 36, 38, 40, 41, 43, 44,
                                          45, 47, 48, 50, 51, 52, 53, 55,
                                          56, 57, 58, 59, 60, 60, 61, 62,
                                          62, 63, 63, 64, 64, 64, 64, 64 };

#if CONFIG_EXT_PARTITION
static const uint8_t obmc_mask_64[64] = {
  33, 34, 35, 35, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 44, 44,
  45, 46, 47, 47, 48, 49, 50, 51, 51, 51, 52, 52, 53, 54, 55, 56,
  56, 56, 57, 57, 58, 58, 59, 60, 60, 60, 60, 60, 61, 62, 62, 62,
  62, 62, 63, 63, 63, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};
#endif  // CONFIG_EXT_PARTITION

const uint8_t *av1_get_obmc_mask(int length) {
  switch (length) {
    case 1: return obmc_mask_1;
    case 2: return obmc_mask_2;
    case 4: return obmc_mask_4;
    case 8: return obmc_mask_8;
    case 16: return obmc_mask_16;
    case 32: return obmc_mask_32;
#if CONFIG_EXT_PARTITION
    case 64: return obmc_mask_64;
#endif  // CONFIG_EXT_PARTITION
    default: assert(0); return NULL;
  }
}

// This function combines motion compensated predictions that is generated by
// top/left neighboring blocks' inter predictors with the regular inter
// prediction. We assume the original prediction (bmc) is stored in
// xd->plane[].dst.buf
void av1_build_obmc_inter_prediction(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col,
                                     uint8_t *above[MAX_MB_PLANE],
                                     int above_stride[MAX_MB_PLANE],
                                     uint8_t *left[MAX_MB_PLANE],
                                     int left_stride[MAX_MB_PLANE]) {
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int plane, i;
#if CONFIG_AOM_HIGHBITDEPTH
  const int is_hbd = (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
#endif  // CONFIG_AOM_HIGHBITDEPTH

  // handle above row
  if (xd->up_available) {
    const int overlap = num_4x4_blocks_high_lookup[bsize] * 2;
    const int miw = AOMMIN(xd->n8_w, cm->mi_cols - mi_col);
    const int mi_row_offset = -1;

    assert(miw > 0);

    i = 0;
    do {  // for each mi in the above row
      const int mi_col_offset = i;
      const MB_MODE_INFO *const above_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;
      const int mi_step =
          AOMMIN(xd->n8_w, num_8x8_blocks_wide_lookup[above_mbmi->sb_type]);

      if (is_neighbor_overlappable(above_mbmi)) {
        for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
          const struct macroblockd_plane *pd = &xd->plane[plane];
          const int bw = (mi_step * MI_SIZE) >> pd->subsampling_x;
          const int bh = overlap >> pd->subsampling_y;
          const int dst_stride = pd->dst.stride;
          uint8_t *const dst = &pd->dst.buf[(i * MI_SIZE) >> pd->subsampling_x];
          const int tmp_stride = above_stride[plane];
          const uint8_t *const tmp =
              &above[plane][(i * MI_SIZE) >> pd->subsampling_x];
          const uint8_t *const mask = av1_get_obmc_mask(bh);

#if CONFIG_AOM_HIGHBITDEPTH
          if (is_hbd)
            aom_highbd_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp,
                                       tmp_stride, mask, bh, bw, xd->bd);
          else
#endif  // CONFIG_AOM_HIGHBITDEPTH
            aom_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp,
                                tmp_stride, mask, bh, bw);
        }
      }
      i += mi_step;
    } while (i < miw);
  }

  // handle left column
  if (xd->left_available) {
    const int overlap = num_4x4_blocks_wide_lookup[bsize] * 2;
    const int mih = AOMMIN(xd->n8_h, cm->mi_rows - mi_row);
    const int mi_col_offset = -1;

    assert(mih > 0);

    i = 0;
    do {  // for each mi in the left column
      const int mi_row_offset = i;
      const MB_MODE_INFO *const left_mbmi =
          &xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride]->mbmi;
      const int mi_step =
          AOMMIN(xd->n8_h, num_8x8_blocks_high_lookup[left_mbmi->sb_type]);

      if (is_neighbor_overlappable(left_mbmi)) {
        for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
          const struct macroblockd_plane *pd = &xd->plane[plane];
          const int bw = overlap >> pd->subsampling_x;
          const int bh = (mi_step * MI_SIZE) >> pd->subsampling_y;
          const int dst_stride = pd->dst.stride;
          uint8_t *const dst =
              &pd->dst.buf[(i * MI_SIZE * dst_stride) >> pd->subsampling_y];
          const int tmp_stride = left_stride[plane];
          const uint8_t *const tmp =
              &left[plane][(i * MI_SIZE * tmp_stride) >> pd->subsampling_y];
          const uint8_t *const mask = av1_get_obmc_mask(bw);

#if CONFIG_AOM_HIGHBITDEPTH
          if (is_hbd)
            aom_highbd_blend_a64_hmask(dst, dst_stride, dst, dst_stride, tmp,
                                       tmp_stride, mask, bh, bw, xd->bd);
          else
#endif  // CONFIG_AOM_HIGHBITDEPTH
            aom_blend_a64_hmask(dst, dst_stride, dst, dst_stride, tmp,
                                tmp_stride, mask, bh, bw);
        }
      }
      i += mi_step;
    } while (i < mih);
  }
}

#if CONFIG_EXT_INTER
void modify_neighbor_predictor_for_obmc(MB_MODE_INFO *mbmi) {
  if (is_interintra_pred(mbmi)) {
    mbmi->ref_frame[1] = NONE;
  } else if (has_second_ref(mbmi) &&
             is_masked_compound_type(mbmi->interinter_compound_data.type)) {
    mbmi->interinter_compound_data.type = COMPOUND_AVERAGE;
    mbmi->ref_frame[1] = NONE;
  }
  return;
}
#endif  // CONFIG_EXT_INTER

void av1_build_prediction_by_above_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row, int mi_col,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         int tmp_width[MAX_MB_PLANE],
                                         int tmp_height[MAX_MB_PLANE],
                                         int tmp_stride[MAX_MB_PLANE]) {
  const TileInfo *const tile = &xd->tile;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int i, j, mi_step, ref;
  int mb_to_right_edge_base = xd->mb_to_right_edge;

  if (mi_row <= tile->mi_row_start) return;

  xd->mb_to_bottom_edge += xd->n8_h * 32;
  for (i = 0; i < AOMMIN(xd->n8_w, cm->mi_cols - mi_col); i += mi_step) {
    int mi_row_offset = -1;
    int mi_col_offset = i;
    int mi_x, mi_y, bw, bh;
    MODE_INFO *above_mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *above_mbmi = &above_mi->mbmi;
#if CONFIG_EXT_INTER
    MB_MODE_INFO backup_mbmi;
#endif  // CONFIG_EXT_INTER

    mi_step = AOMMIN(xd->n8_w, num_8x8_blocks_wide_lookup[above_mbmi->sb_type]);

    if (!is_neighbor_overlappable(above_mbmi)) continue;

#if CONFIG_EXT_INTER
    backup_mbmi = *above_mbmi;
    modify_neighbor_predictor_for_obmc(above_mbmi);
#endif  // CONFIG_EXT_INTER

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      struct macroblockd_plane *const pd = &xd->plane[j];
      setup_pred_plane(&pd->dst, tmp_buf[j], tmp_width[j], tmp_height[j],
                       tmp_stride[j], 0, i, NULL, pd->subsampling_x,
                       pd->subsampling_y);
    }
    for (ref = 0; ref < 1 + has_second_ref(above_mbmi); ++ref) {
      const MV_REFERENCE_FRAME frame = above_mbmi->ref_frame[ref];
      const RefBuffer *const ref_buf = &cm->frame_refs[frame - LAST_FRAME];

      xd->block_refs[ref] = ref_buf;
      if ((!av1_is_valid_scale(&ref_buf->sf)))
        aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reference frame has invalid dimensions");
      av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row, mi_col + i,
                           &ref_buf->sf);
    }

    xd->mb_to_left_edge = -(((mi_col + i) * MI_SIZE) * 8);
    xd->mb_to_right_edge =
        mb_to_right_edge_base + (xd->n8_w - i - mi_step) * 64;
    mi_x = (mi_col + i) << MI_SIZE_LOG2;
    mi_y = mi_row << MI_SIZE_LOG2;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      const struct macroblockd_plane *pd = &xd->plane[j];
      bw = (mi_step * 8) >> pd->subsampling_x;
      bh = AOMMAX((num_4x4_blocks_high_lookup[bsize] * 2) >> pd->subsampling_y,
                  4);

      if (above_mbmi->sb_type < BLOCK_8X8) {
        const PARTITION_TYPE bp = BLOCK_8X8 - above_mbmi->sb_type;
        const int have_vsplit = bp != PARTITION_HORZ;
        const int have_hsplit = bp != PARTITION_VERT;
        const int num_4x4_w = 2 >> !have_vsplit;
        const int num_4x4_h = 2 >> !have_hsplit;
        const int pw = 8 >> (have_vsplit + pd->subsampling_x);
        int x, y;

        for (y = 0; y < num_4x4_h; ++y)
          for (x = 0; x < num_4x4_w; ++x) {
            if ((bp == PARTITION_HORZ || bp == PARTITION_SPLIT) && y == 0)
              continue;

            build_inter_predictors(xd, j, mi_col_offset, mi_row_offset,
                                   y * 2 + x, bw, bh,
                                   (4 * x) >> pd->subsampling_x, 0, pw, bh,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                                   0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                                   mi_x, mi_y);
          }
      } else {
#if CONFIG_WARPED_MOTION
        if (above_mbmi->motion_mode == WARPED_CAUSAL) {
          av1_warp_plane(&above_mbmi->wm_params[0],
#if CONFIG_AOM_HIGHBITDEPTH
                         xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                         pd->pre[0].buf0, pd->pre[0].width, pd->pre[0].height,
                         pd->pre[0].stride, pd->dst.buf,
                         (((mi_col + i) * MI_SIZE) >> pd->subsampling_x),
                         ((mi_row * MI_SIZE) >> pd->subsampling_y), bw, bh,
                         pd->dst.stride, pd->subsampling_x, pd->subsampling_y,
                         16, 16, 0);

        } else {
#endif  // CONFIG_WARPED_MOTION
          build_inter_predictors(xd, j, mi_col_offset, mi_row_offset, 0, bw, bh,
                                 0, 0, bw, bh,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 mi_x, mi_y);
#if CONFIG_WARPED_MOTION
        }
#endif  // CONFIG_WARPED_MOTION
      }
    }
#if CONFIG_EXT_INTER
    *above_mbmi = backup_mbmi;
#endif  // CONFIG_EXT_INTER
  }
  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = mb_to_right_edge_base;
  xd->mb_to_bottom_edge -= xd->n8_h * 32;
}

void av1_build_prediction_by_left_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        int tmp_width[MAX_MB_PLANE],
                                        int tmp_height[MAX_MB_PLANE],
                                        int tmp_stride[MAX_MB_PLANE]) {
  const TileInfo *const tile = &xd->tile;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  int i, j, mi_step, ref;
  int mb_to_bottom_edge_base = xd->mb_to_bottom_edge;

  if (mi_col == 0 || (mi_col - 1 < tile->mi_col_start)) return;

  xd->mb_to_right_edge += xd->n8_w * 32;
  for (i = 0; i < AOMMIN(xd->n8_h, cm->mi_rows - mi_row); i += mi_step) {
    int mi_row_offset = i;
    int mi_col_offset = -1;
    int mi_x, mi_y, bw, bh;
    MODE_INFO *left_mi = xd->mi[mi_col_offset + mi_row_offset * xd->mi_stride];
    MB_MODE_INFO *left_mbmi = &left_mi->mbmi;
#if CONFIG_EXT_INTER
    MB_MODE_INFO backup_mbmi;
#endif  // CONFIG_EXT_INTER

    mi_step = AOMMIN(xd->n8_h, num_8x8_blocks_high_lookup[left_mbmi->sb_type]);

    if (!is_neighbor_overlappable(left_mbmi)) continue;

#if CONFIG_EXT_INTER
    backup_mbmi = *left_mbmi;
    modify_neighbor_predictor_for_obmc(left_mbmi);
#endif  // CONFIG_EXT_INTER

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      struct macroblockd_plane *const pd = &xd->plane[j];
      setup_pred_plane(&pd->dst, tmp_buf[j], tmp_width[j], tmp_height[j],
                       tmp_stride[j], i, 0, NULL, pd->subsampling_x,
                       pd->subsampling_y);
    }
    for (ref = 0; ref < 1 + has_second_ref(left_mbmi); ++ref) {
      const MV_REFERENCE_FRAME frame = left_mbmi->ref_frame[ref];
      const RefBuffer *const ref_buf = &cm->frame_refs[frame - LAST_FRAME];

      xd->block_refs[ref] = ref_buf;
      if ((!av1_is_valid_scale(&ref_buf->sf)))
        aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                           "Reference frame has invalid dimensions");
      av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row + i, mi_col,
                           &ref_buf->sf);
    }

    xd->mb_to_top_edge = -(((mi_row + i) * MI_SIZE) * 8);
    xd->mb_to_bottom_edge =
        mb_to_bottom_edge_base + (xd->n8_h - i - mi_step) * 64;
    mi_x = mi_col << MI_SIZE_LOG2;
    mi_y = (mi_row + i) << MI_SIZE_LOG2;

    for (j = 0; j < MAX_MB_PLANE; ++j) {
      const struct macroblockd_plane *pd = &xd->plane[j];
      bw = AOMMAX((num_4x4_blocks_wide_lookup[bsize] * 2) >> pd->subsampling_x,
                  4);
      bh = (mi_step << MI_SIZE_LOG2) >> pd->subsampling_y;

      if (left_mbmi->sb_type < BLOCK_8X8) {
        const PARTITION_TYPE bp = BLOCK_8X8 - left_mbmi->sb_type;
        const int have_vsplit = bp != PARTITION_HORZ;
        const int have_hsplit = bp != PARTITION_VERT;
        const int num_4x4_w = 2 >> !have_vsplit;
        const int num_4x4_h = 2 >> !have_hsplit;
        const int ph = 8 >> (have_hsplit + pd->subsampling_y);
        int x, y;

        for (y = 0; y < num_4x4_h; ++y)
          for (x = 0; x < num_4x4_w; ++x) {
            if ((bp == PARTITION_VERT || bp == PARTITION_SPLIT) && x == 0)
              continue;

            build_inter_predictors(xd, j, mi_col_offset, mi_row_offset,
                                   y * 2 + x, bw, bh, 0,
                                   (4 * y) >> pd->subsampling_y, bw, ph,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                                   0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                                   mi_x, mi_y);
          }
      } else {
#if CONFIG_WARPED_MOTION
        if (left_mbmi->motion_mode == WARPED_CAUSAL) {
          av1_warp_plane(&left_mbmi->wm_params[0],
#if CONFIG_AOM_HIGHBITDEPTH
                         xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH, xd->bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                         pd->pre[0].buf0, pd->pre[0].width, pd->pre[0].height,
                         pd->pre[0].stride, pd->dst.buf,
                         ((mi_col * MI_SIZE) >> pd->subsampling_x),
                         (((mi_row + i) * MI_SIZE) >> pd->subsampling_y), bw,
                         bh, pd->dst.stride, pd->subsampling_x,
                         pd->subsampling_y, 16, 16, 0);

        } else {
#endif  // CONFIG_WARPED_MOTION
          build_inter_predictors(xd, j, mi_col_offset, mi_row_offset, 0, bw, bh,
                                 0, 0, bw, bh,
#if CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 0, 0,
#endif  // CONFIG_SUPERTX && CONFIG_EXT_INTER
                                 mi_x, mi_y);
#if CONFIG_WARPED_MOTION
        }
#endif  // CONFIG_WARPED_MOTION
      }
    }
#if CONFIG_EXT_INTER
    *left_mbmi = backup_mbmi;
#endif  // CONFIG_EXT_INTER
  }
  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_bottom_edge = mb_to_bottom_edge_base;
  xd->mb_to_right_edge -= xd->n8_w * 32;
}

void av1_build_obmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col) {
#if CONFIG_AOM_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[2 * MAX_MB_PLANE * MAX_SB_SQUARE]);
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf1[MAX_MB_PLANE * MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, tmp_buf2[MAX_MB_PLANE * MAX_SB_SQUARE]);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(tmp_buf1);
    dst_buf1[1] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_SB_SQUARE * len);
    dst_buf1[2] = CONVERT_TO_BYTEPTR(tmp_buf1 + MAX_SB_SQUARE * 2 * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(tmp_buf2);
    dst_buf2[1] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_SB_SQUARE * len);
    dst_buf2[2] = CONVERT_TO_BYTEPTR(tmp_buf2 + MAX_SB_SQUARE * 2 * len);
  } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
    dst_buf1[0] = tmp_buf1;
    dst_buf1[1] = tmp_buf1 + MAX_SB_SQUARE;
    dst_buf1[2] = tmp_buf1 + MAX_SB_SQUARE * 2;
    dst_buf2[0] = tmp_buf2;
    dst_buf2[1] = tmp_buf2 + MAX_SB_SQUARE;
    dst_buf2[2] = tmp_buf2 + MAX_SB_SQUARE * 2;
#if CONFIG_AOM_HIGHBITDEPTH
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  av1_build_prediction_by_above_preds(cm, xd, mi_row, mi_col, dst_buf1,
                                      dst_width1, dst_height1, dst_stride1);
  av1_build_prediction_by_left_preds(cm, xd, mi_row, mi_col, dst_buf2,
                                     dst_width2, dst_height2, dst_stride2);
  av1_setup_dst_planes(xd->plane, get_frame_new_buffer(cm), mi_row, mi_col);
  av1_build_obmc_inter_prediction(cm, xd, mi_row, mi_col, dst_buf1, dst_stride1,
                                  dst_buf2, dst_stride2);
}
#endif  // CONFIG_MOTION_VAR

#if CONFIG_EXT_INTER
#if CONFIG_EXT_PARTITION
static const int ii_weights1d[MAX_SB_SIZE] = {
  102, 100, 97, 95, 92, 90, 88, 86, 84, 82, 80, 78, 76, 74, 73, 71, 69, 68, 67,
  65,  64,  62, 61, 60, 59, 58, 57, 55, 54, 53, 52, 52, 51, 50, 49, 48, 47, 47,
  46,  45,  45, 44, 43, 43, 42, 41, 41, 40, 40, 39, 39, 38, 38, 38, 37, 37, 36,
  36,  36,  35, 35, 35, 34, 34, 34, 33, 33, 33, 33, 32, 32, 32, 32, 32, 31, 31,
  31,  31,  31, 30, 30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 28,
  28,  28,  28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27, 27, 27, 27,
  27,  27,  27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
};
static int ii_size_scales[BLOCK_SIZES] = { 32, 16, 16, 16, 8, 8, 8, 4,
                                           4,  4,  2,  2,  2, 1, 1, 1 };
#else
static const int ii_weights1d[MAX_SB_SIZE] = {
  102, 100, 97, 95, 92, 90, 88, 86, 84, 82, 80, 78, 76, 74, 73, 71,
  69,  68,  67, 65, 64, 62, 61, 60, 59, 58, 57, 55, 54, 53, 52, 52,
  51,  50,  49, 48, 47, 47, 46, 45, 45, 44, 43, 43, 42, 41, 41, 40,
  40,  39,  39, 38, 38, 38, 37, 37, 36, 36, 36, 35, 35, 35, 34, 34,
};
static int ii_size_scales[BLOCK_SIZES] = { 16, 8, 8, 8, 4, 4, 4,
                                           2,  2, 2, 1, 1, 1 };
#endif  // CONFIG_EXT_PARTITION

static void combine_interintra(INTERINTRA_MODE mode, int use_wedge_interintra,
                               int wedge_index, int wedge_sign,
                               BLOCK_SIZE bsize, BLOCK_SIZE plane_bsize,
                               uint8_t *comppred, int compstride,
                               const uint8_t *interpred, int interstride,
                               const uint8_t *intrapred, int intrastride) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];
  const int size_scale = ii_size_scales[plane_bsize];
  int i, j;

  if (use_wedge_interintra) {
    if (is_interintra_wedge_used(bsize)) {
      const uint8_t *mask =
          av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
      const int subw = 2 * num_4x4_blocks_wide_lookup[bsize] == bw;
      const int subh = 2 * num_4x4_blocks_high_lookup[bsize] == bh;
      aom_blend_a64_mask(comppred, compstride, intrapred, intrastride,
                         interpred, interstride, mask, block_size_wide[bsize],
                         bh, bw, subh, subw);
    }
    return;
  }

  switch (mode) {
    case II_V_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[i * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_H_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[j * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D63_PRED:
    case II_D117_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = (ii_weights1d[i * size_scale] * 3 +
                       ii_weights1d[j * size_scale]) >>
                      2;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D207_PRED:
    case II_D153_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = (ii_weights1d[j * size_scale] * 3 +
                       ii_weights1d[i * size_scale]) >>
                      2;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D135_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[(i < j ? i : j) * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D45_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale =
              (ii_weights1d[i * size_scale] + ii_weights1d[j * size_scale]) >>
              1;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_TM_PRED:
    case II_DC_PRED:
    default:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          comppred[i * compstride + j] = AOM_BLEND_AVG(
              intrapred[i * intrastride + j], interpred[i * interstride + j]);
        }
      }
      break;
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
static void combine_interintra_highbd(
    INTERINTRA_MODE mode, int use_wedge_interintra, int wedge_index,
    int wedge_sign, BLOCK_SIZE bsize, BLOCK_SIZE plane_bsize,
    uint8_t *comppred8, int compstride, const uint8_t *interpred8,
    int interstride, const uint8_t *intrapred8, int intrastride, int bd) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];
  const int size_scale = ii_size_scales[plane_bsize];
  int i, j;

  uint16_t *comppred = CONVERT_TO_SHORTPTR(comppred8);
  const uint16_t *interpred = CONVERT_TO_SHORTPTR(interpred8);
  const uint16_t *intrapred = CONVERT_TO_SHORTPTR(intrapred8);

  if (use_wedge_interintra) {
    if (is_interintra_wedge_used(bsize)) {
      const uint8_t *mask =
          av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
      const int subh = 2 * num_4x4_blocks_high_lookup[bsize] == bh;
      const int subw = 2 * num_4x4_blocks_wide_lookup[bsize] == bw;
      aom_highbd_blend_a64_mask(comppred8, compstride, intrapred8, intrastride,
                                interpred8, interstride, mask, bw, bh, bw, subh,
                                subw, bd);
    }
    return;
  }

  switch (mode) {
    case II_V_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[i * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_H_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[j * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D63_PRED:
    case II_D117_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = (ii_weights1d[i * size_scale] * 3 +
                       ii_weights1d[j * size_scale]) >>
                      2;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D207_PRED:
    case II_D153_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = (ii_weights1d[j * size_scale] * 3 +
                       ii_weights1d[i * size_scale]) >>
                      2;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D135_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale = ii_weights1d[(i < j ? i : j) * size_scale];
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_D45_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          int scale =
              (ii_weights1d[i * size_scale] + ii_weights1d[j * size_scale]) >>
              1;
          comppred[i * compstride + j] =
              AOM_BLEND_A256(scale, intrapred[i * intrastride + j],
                             interpred[i * interstride + j]);
        }
      }
      break;

    case II_TM_PRED:
    case II_DC_PRED:
    default:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) {
          comppred[i * compstride + j] = AOM_BLEND_AVG(
              interpred[i * interstride + j], intrapred[i * intrastride + j]);
        }
      }
      break;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

static void build_intra_predictors_for_interintra(MACROBLOCKD *xd, uint8_t *ref,
                                                  int ref_stride, uint8_t *dst,
                                                  int dst_stride,
                                                  PREDICTION_MODE mode,
                                                  BLOCK_SIZE bsize, int plane) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, &xd->plane[plane]);
  const int bwl = b_width_log2_lookup[plane_bsize];
  const int bhl = b_height_log2_lookup[plane_bsize];
  TX_SIZE max_tx_size = max_txsize_lookup[plane_bsize];
#if USE_RECT_INTERINTRA
  const int pxbw = 4 << bwl;
  const int pxbh = 4 << bhl;
#if CONFIG_AOM_HIGHBITDEPTH
  uint16_t tmp16[MAX_SB_SIZE];
#endif
  uint8_t tmp[MAX_SB_SIZE];
#endif

  if (bwl == bhl) {
    av1_predict_intra_block(xd, pd->width, pd->height, max_tx_size, mode, ref,
                            ref_stride, dst, dst_stride, 0, 0, plane);
#if !USE_RECT_INTERINTRA
  } else {
    assert(0);
  }
#else
  } else if (bwl < bhl) {
    uint8_t *src_2 = ref + pxbw * ref_stride;
    uint8_t *dst_2 = dst + pxbw * dst_stride;
    av1_predict_intra_block(xd, pd->width, pd->height, max_tx_size, mode, ref,
                            ref_stride, dst, dst_stride, 0, 0, plane);
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *src_216 = CONVERT_TO_SHORTPTR(src_2);
      uint16_t *dst_216 = CONVERT_TO_SHORTPTR(dst_2);
      memcpy(tmp16, src_216 - ref_stride, sizeof(*src_216) * pxbw);
      memcpy(src_216 - ref_stride, dst_216 - dst_stride,
             sizeof(*src_216) * pxbw);
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      memcpy(tmp, src_2 - ref_stride, sizeof(*src_2) * pxbw);
      memcpy(src_2 - ref_stride, dst_2 - dst_stride, sizeof(*src_2) * pxbw);
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif
    av1_predict_intra_block(xd, pd->width, pd->height, max_tx_size, mode, src_2,
                            ref_stride, dst_2, dst_stride, 0, 1 << bwl, plane);
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *src_216 = CONVERT_TO_SHORTPTR(src_2);
      memcpy(src_216 - ref_stride, tmp16, sizeof(*src_216) * pxbw);
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      memcpy(src_2 - ref_stride, tmp, sizeof(*src_2) * pxbw);
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif
  } else {  // bwl > bhl
    int i;
    uint8_t *src_2 = ref + pxbh;
    uint8_t *dst_2 = dst + pxbh;
    av1_predict_intra_block(xd, pd->width, pd->height, max_tx_size, mode, ref,
                            ref_stride, dst, dst_stride, 0, 0, plane);
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *src_216 = CONVERT_TO_SHORTPTR(src_2);
      uint16_t *dst_216 = CONVERT_TO_SHORTPTR(dst_2);
      for (i = 0; i < pxbh; ++i) {
        tmp16[i] = src_216[i * ref_stride - 1];
        src_216[i * ref_stride - 1] = dst_216[i * dst_stride - 1];
      }
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      for (i = 0; i < pxbh; ++i) {
        tmp[i] = src_2[i * ref_stride - 1];
        src_2[i * ref_stride - 1] = dst_2[i * dst_stride - 1];
      }
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif
    av1_predict_intra_block(xd, pd->width, pd->height, max_tx_size, mode, src_2,
                            ref_stride, dst_2, dst_stride, 1 << bhl, 0, plane);
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *src_216 = CONVERT_TO_SHORTPTR(src_2);
      for (i = 0; i < pxbh; ++i) src_216[i * ref_stride - 1] = tmp16[i];
    } else {
#endif  // CONFIG_AOM_HIGHBITDEPTH
      for (i = 0; i < pxbh; ++i) src_2[i * ref_stride - 1] = tmp[i];
#if CONFIG_AOM_HIGHBITDEPTH
    }
#endif
  }
#endif
}

void av1_build_intra_predictors_for_interintra(MACROBLOCKD *xd,
                                               BLOCK_SIZE bsize, int plane,
                                               BUFFER_SET *ctx, uint8_t *dst,
                                               int dst_stride) {
  build_intra_predictors_for_interintra(
      xd, ctx->plane[plane], ctx->stride[plane], dst, dst_stride,
      interintra_to_intra_mode[xd->mi[0]->mbmi.interintra_mode], bsize, plane);
}

void av1_combine_interintra(MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                            const uint8_t *inter_pred, int inter_stride,
                            const uint8_t *intra_pred, int intra_stride) {
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, &xd->plane[plane]);
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    combine_interintra_highbd(
        xd->mi[0]->mbmi.interintra_mode, xd->mi[0]->mbmi.use_wedge_interintra,
        xd->mi[0]->mbmi.interintra_wedge_index,
        xd->mi[0]->mbmi.interintra_wedge_sign, bsize, plane_bsize,
        xd->plane[plane].dst.buf, xd->plane[plane].dst.stride, inter_pred,
        inter_stride, intra_pred, intra_stride, xd->bd);
    return;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  combine_interintra(xd->mi[0]->mbmi.interintra_mode,
                     xd->mi[0]->mbmi.use_wedge_interintra,
                     xd->mi[0]->mbmi.interintra_wedge_index,
                     xd->mi[0]->mbmi.interintra_wedge_sign, bsize, plane_bsize,
                     xd->plane[plane].dst.buf, xd->plane[plane].dst.stride,
                     inter_pred, inter_stride, intra_pred, intra_stride);
}

void av1_build_interintra_predictors_sby(MACROBLOCKD *xd, uint8_t *ypred,
                                         int ystride, BUFFER_SET *ctx,
                                         BLOCK_SIZE bsize) {
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    DECLARE_ALIGNED(16, uint16_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(
        xd, bsize, 0, ctx, CONVERT_TO_BYTEPTR(intrapredictor), MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, 0, ypred, ystride,
                           CONVERT_TO_BYTEPTR(intrapredictor), MAX_SB_SIZE);
    return;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  {
    DECLARE_ALIGNED(16, uint8_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(xd, bsize, 0, ctx, intrapredictor,
                                              MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, 0, ypred, ystride, intrapredictor,
                           MAX_SB_SIZE);
  }
}

void av1_build_interintra_predictors_sbc(MACROBLOCKD *xd, uint8_t *upred,
                                         int ustride, BUFFER_SET *ctx,
                                         int plane, BLOCK_SIZE bsize) {
#if CONFIG_AOM_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    DECLARE_ALIGNED(16, uint16_t, uintrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(
        xd, bsize, plane, ctx, CONVERT_TO_BYTEPTR(uintrapredictor),
        MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, upred, ustride,
                           CONVERT_TO_BYTEPTR(uintrapredictor), MAX_SB_SIZE);
    return;
  }
#endif  // CONFIG_AOM_HIGHBITDEPTH
  {
    DECLARE_ALIGNED(16, uint8_t, uintrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(xd, bsize, plane, ctx,
                                              uintrapredictor, MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, upred, ustride, uintrapredictor,
                           MAX_SB_SIZE);
  }
}

void av1_build_interintra_predictors_sbuv(MACROBLOCKD *xd, uint8_t *upred,
                                          uint8_t *vpred, int ustride,
                                          int vstride, BUFFER_SET *ctx,
                                          BLOCK_SIZE bsize) {
  av1_build_interintra_predictors_sbc(xd, upred, ustride, ctx, 1, bsize);
  av1_build_interintra_predictors_sbc(xd, vpred, vstride, ctx, 2, bsize);
}

void av1_build_interintra_predictors(MACROBLOCKD *xd, uint8_t *ypred,
                                     uint8_t *upred, uint8_t *vpred,
                                     int ystride, int ustride, int vstride,
                                     BUFFER_SET *ctx, BLOCK_SIZE bsize) {
  av1_build_interintra_predictors_sby(xd, ypred, ystride, ctx, bsize);
  av1_build_interintra_predictors_sbuv(xd, upred, vpred, ustride, vstride, ctx,
                                       bsize);
}

// Builds the inter-predictor for the single ref case
// for use in the encoder to search the wedges efficiently.
static void build_inter_predictors_single_buf(MACROBLOCKD *xd, int plane,
                                              int block, int bw, int bh, int x,
                                              int y, int w, int h, int mi_x,
                                              int mi_y, int ref,
                                              uint8_t *const ext_dst,
                                              int ext_dst_stride) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const MODE_INFO *mi = xd->mi[0];

  const struct scale_factors *const sf = &xd->block_refs[ref]->sf;
  struct buf_2d *const pre_buf = &pd->pre[ref];
#if CONFIG_AOM_HIGHBITDEPTH
  uint8_t *const dst =
      (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH ? CONVERT_TO_BYTEPTR(ext_dst)
                                                   : ext_dst) +
      ext_dst_stride * y + x;
#else
  uint8_t *const dst = ext_dst + ext_dst_stride * y + x;
#endif
  const MV mv = mi->mbmi.sb_type < BLOCK_8X8
                    ? average_split_mvs(pd, mi, ref, block)
                    : mi->mbmi.mv[ref].as_mv;

  // TODO(jkoleszar): This clamping is done in the incorrect place for the
  // scaling case. It needs to be done on the scaled MV, not the pre-scaling
  // MV. Note however that it performs the subsampling aware scaling so
  // that the result is always q4.
  // mv_precision precision is MV_PRECISION_Q4.
  const MV mv_q4 = clamp_mv_to_umv_border_sb(xd, &mv, bw, bh, pd->subsampling_x,
                                             pd->subsampling_y);

  uint8_t *pre;
  MV32 scaled_mv;
  int xs, ys, subpel_x, subpel_y;
  const int is_scaled = av1_is_scaled(sf);

  if (is_scaled) {
    pre = pre_buf->buf + scaled_buffer_offset(x, y, pre_buf->stride, sf);
    scaled_mv = av1_scale_mv(&mv_q4, mi_x + x, mi_y + y, sf);
    xs = sf->x_step_q4;
    ys = sf->y_step_q4;
  } else {
    pre = pre_buf->buf + (y * pre_buf->stride + x);
    scaled_mv.row = mv_q4.row;
    scaled_mv.col = mv_q4.col;
    xs = ys = 16;
  }

  subpel_x = scaled_mv.col & SUBPEL_MASK;
  subpel_y = scaled_mv.row & SUBPEL_MASK;
  pre += (scaled_mv.row >> SUBPEL_BITS) * pre_buf->stride +
         (scaled_mv.col >> SUBPEL_BITS);

  av1_make_inter_predictor(pre, pre_buf->stride, dst, ext_dst_stride, subpel_x,
                           subpel_y, sf, w, h, 0, mi->mbmi.interp_filter, xs,
                           ys, xd);
}

void av1_build_inter_predictors_for_planes_single_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to, int mi_row,
    int mi_col, int ref, uint8_t *ext_dst[3], int ext_dst_stride[3]) {
  int plane;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, &xd->plane[plane]);
    const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];

    if (xd->mi[0]->mbmi.sb_type < BLOCK_8X8) {
      int x, y;
      assert(bsize == BLOCK_8X8);
      for (y = 0; y < num_4x4_h; ++y)
        for (x = 0; x < num_4x4_w; ++x)
          build_inter_predictors_single_buf(
              xd, plane, y * 2 + x, bw, bh, 4 * x, 4 * y, 4, 4, mi_x, mi_y, ref,
              ext_dst[plane], ext_dst_stride[plane]);
    } else {
      build_inter_predictors_single_buf(xd, plane, 0, bw, bh, 0, 0, bw, bh,
                                        mi_x, mi_y, ref, ext_dst[plane],
                                        ext_dst_stride[plane]);
    }
  }
}

static void build_wedge_inter_predictor_from_buf(
    MACROBLOCKD *xd, int plane, int x, int y, int w, int h, uint8_t *ext_dst0,
    int ext_dst_stride0, uint8_t *ext_dst1, int ext_dst_stride1) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_compound = has_second_ref(mbmi);
  MACROBLOCKD_PLANE *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;
  uint8_t *const dst = dst_buf->buf + dst_buf->stride * y + x;
  const INTERINTER_COMPOUND_DATA *const comp_data =
      &mbmi->interinter_compound_data;

  if (is_compound &&
      is_masked_compound_type(mbmi->interinter_compound_data.type)) {
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      build_masked_compound_wedge_highbd(
          dst, dst_buf->stride, CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
          CONVERT_TO_BYTEPTR(ext_dst1), ext_dst_stride1, comp_data->wedge_index,
          comp_data->wedge_sign, mbmi->sb_type, h, w, xd->bd);
    else
#endif  // CONFIG_AOM_HIGHBITDEPTH
      build_masked_compound(dst, dst_buf->stride, ext_dst0, ext_dst_stride0,
                            ext_dst1, ext_dst_stride1, comp_data, mbmi->sb_type,
                            h, w);
  } else {
#if CONFIG_AOM_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
      aom_highbd_convolve_copy(CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
                               dst, dst_buf->stride, NULL, 0, NULL, 0, w, h,
                               xd->bd);
    else
#endif  // CONFIG_AOM_HIGHBITDEPTH
      aom_convolve_copy(ext_dst0, ext_dst_stride0, dst, dst_buf->stride, NULL,
                        0, NULL, 0, w, h);
  }
}

void av1_build_wedge_inter_predictor_from_buf(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int plane_from, int plane_to,
                                              uint8_t *ext_dst0[3],
                                              int ext_dst_stride0[3],
                                              uint8_t *ext_dst1[3],
                                              int ext_dst_stride1[3]) {
  int plane;
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, &xd->plane[plane]);
    const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];

    if (xd->mi[0]->mbmi.sb_type < BLOCK_8X8) {
      int x, y;
      assert(bsize == BLOCK_8X8);
      for (y = 0; y < num_4x4_h; ++y)
        for (x = 0; x < num_4x4_w; ++x)
          build_wedge_inter_predictor_from_buf(
              xd, plane, 4 * x, 4 * y, 4, 4, ext_dst0[plane],
              ext_dst_stride0[plane], ext_dst1[plane], ext_dst_stride1[plane]);
    } else {
      const int bw = 4 * num_4x4_w;
      const int bh = 4 * num_4x4_h;
      build_wedge_inter_predictor_from_buf(
          xd, plane, 0, 0, bw, bh, ext_dst0[plane], ext_dst_stride0[plane],
          ext_dst1[plane], ext_dst_stride1[plane]);
    }
  }
}
#endif  // CONFIG_EXT_INTER
