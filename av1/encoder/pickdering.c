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

#include <string.h>

#include "./aom_scale_rtcd.h"
#include "av1/common/dering.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/encoder.h"
#include "aom/aom_integer.h"

static double compute_dist(int16_t *x, int xstride, int16_t *y, int ystride,
    int nhb, int nvb, int coeff_shift) {
  int i, j;
  double sum;
  sum = 0;
  for (i = 0; i < nvb << 3; i++) {
    for (j = 0; j < nhb << 3; j++) {
      double tmp;
      tmp = x[i*xstride + j] - y[i*ystride + j];
      sum += tmp*tmp;
    }
  }
  return sum/(double)(1 << 2*coeff_shift);
}

int av1_dering_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                       AV1_COMMON *cm,
                       MACROBLOCKD *xd) {
  int r, c;
  int sbr, sbc;
  int nhsb, nvsb;
  od_dering_in *src;
  int16_t *ref_coeff;
  unsigned char *bskip;
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = {{0}};
  int stride;
  int bsize[3];
  int dec[3];
  int pli;
  int (*mse)[MAX_DERING_LEVEL];
  double tot_mse[MAX_DERING_LEVEL] = {0};
  int level;
  int best_level;
  int global_level;
  double best_tot_mse = 1e15;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  src = aom_malloc(sizeof(*src)*cm->mi_rows*cm->mi_cols*64);
  ref_coeff = aom_malloc(sizeof(*ref_coeff)*cm->mi_rows*cm->mi_cols*64);
  bskip = aom_malloc(sizeof(*bskip)*cm->mi_rows*cm->mi_cols);
  av1_setup_dst_planes(xd->plane, frame, 0, 0);
  for (pli = 0; pli < 3; pli++) {
    dec[pli] = xd->plane[pli].subsampling_x;
    bsize[pli] = 8 >> dec[pli];
  }
  stride = bsize[0]*cm->mi_cols;
  for (r = 0; r < bsize[0]*cm->mi_rows; ++r) {
    for (c = 0; c < bsize[0]*cm->mi_cols; ++c) {
#if CONFIG_AOM_HIGHBITDEPTH
      if (cm->use_highbitdepth) {
        src[r * stride + c] =
            CONVERT_TO_SHORTPTR(xd->plane[0].dst.buf)
            [r*xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] =
            CONVERT_TO_SHORTPTR(ref->y_buffer)[r * ref->y_stride + c];
      } else {
#endif
        src[r * stride + c] =
            xd->plane[0].dst.buf[r*xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] = ref->y_buffer[r * ref->y_stride + c];
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif
    }
  }
  for (r = 0; r < cm->mi_rows; ++r) {
    for (c = 0; c < cm->mi_cols; ++c) {
      const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
      bskip[r * cm->mi_cols + c] = mbmi->skip;
    }
  }
  nvsb = (cm->mi_rows + MI_BLOCK_SIZE - 1)/MI_BLOCK_SIZE;
  nhsb = (cm->mi_cols + MI_BLOCK_SIZE - 1)/MI_BLOCK_SIZE;
  mse = aom_malloc(nvsb*nhsb*sizeof(*mse));
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (sbc = 0; sbc < nhsb; sbc++) {
      int nvb, nhb;
      int16_t dst[MI_BLOCK_SIZE*MI_BLOCK_SIZE*8*8];
      nhb = AOMMIN(MI_BLOCK_SIZE, cm->mi_cols - MI_BLOCK_SIZE*sbc);
      nvb = AOMMIN(MI_BLOCK_SIZE, cm->mi_rows - MI_BLOCK_SIZE*sbr);
      for (level = 0; level < 64; level++) {
        int threshold;
        threshold = level << coeff_shift;
        od_dering(
            &OD_DERING_VTBL_C,
            dst,
            MI_BLOCK_SIZE*bsize[0],
            &src[sbr*stride*bsize[0]*MI_BLOCK_SIZE +
            sbc*bsize[0]*MI_BLOCK_SIZE],
            cm->mi_cols*bsize[0], nhb, nvb, sbc, sbr, nhsb, nvsb, 0, dir, 0,
            &bskip[MI_BLOCK_SIZE*sbr*cm->mi_cols + MI_BLOCK_SIZE*sbc],
            cm->mi_cols, threshold, OD_DERING_NO_CHECK_OVERLAP, coeff_shift);
        int cur_mse = compute_dist(
            dst, MI_BLOCK_SIZE*bsize[0],
            &ref_coeff[sbr*stride*bsize[0]*MI_BLOCK_SIZE +
            sbc*bsize[0]*MI_BLOCK_SIZE],
            stride, nhb, nvb, coeff_shift);
        mse[nhsb*sbr+sbc][level] = cur_mse;
        tot_mse[level] += cur_mse;
      }
    }
  }
#if DERING_REFINEMENT
  best_level = 0;
  /* Search for the best global level one value at a time. */
  for (global_level = 2; global_level < MAX_DERING_LEVEL; global_level++) {
    double tot_mse = 0;
    for (sbr = 0; sbr < nvsb; sbr++) {
      for (sbc = 0; sbc < nhsb; sbc++) {
        int gi;
        int best_mse = mse[nhsb*sbr+sbc][0];
        for (gi = 1; gi < 4; gi++) {
          level = compute_level_from_index(global_level, gi);
          if (mse[nhsb*sbr+sbc][level] < best_mse) {
            best_mse = mse[nhsb*sbr+sbc][level];
          }
        }
        tot_mse += best_mse;
      }
    }
    if (tot_mse < best_tot_mse) {
      best_level = global_level;
      best_tot_mse = tot_mse;
    }
  }
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (sbc = 0; sbc < nhsb; sbc++) {
      int gi;
      int best_gi;
      int best_mse = mse[nhsb*sbr+sbc][0];
      best_gi = 0;
      for (gi = 1; gi < DERING_REFINEMENT_LEVELS; gi++) {
        level = compute_level_from_index(best_level, gi);
        if (mse[nhsb*sbr+sbc][level] < best_mse) {
          best_gi = gi;
          best_mse = mse[nhsb*sbr+sbc][level];
        }
      }
      cm->mi_grid_visible[MI_BLOCK_SIZE*sbr*cm->mi_stride + MI_BLOCK_SIZE*sbc]->
          mbmi.dering_gain = best_gi;
    }
  }
#else
  best_level = 0;
  for (level = 0; level < MAX_DERING_LEVEL; level++) {
    if (tot_mse[level] < tot_mse[best_level]) best_level = level;
  }
#endif
  aom_free(src);
  aom_free(ref_coeff);
  aom_free(bskip);
  aom_free(mse);
  return best_level;
}
