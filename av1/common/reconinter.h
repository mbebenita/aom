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

#ifndef AV1_COMMON_RECONINTER_H_
#define AV1_COMMON_RECONINTER_H_

#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE void inter_predictor(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const int subpel_x, const int subpel_y,
                                   const struct scale_factors *sf, int w, int h,
                                   int ref, const InterpFilter *interp_filter,
                                   int xs, int ys) {
  InterpFilterParams interp_filter_params =
      get_interp_filter_params(*interp_filter);
  if (interp_filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        get_interp_filter_subpel_kernel(interp_filter_params, subpel_x);
    const int16_t *filter_y =
        get_interp_filter_subpel_kernel(interp_filter_params, subpel_y);

    sf->predict[subpel_x != 0][subpel_y != 0][ref](
        src, src_stride, dst, dst_stride, filter_x, xs, filter_y, ys, w, h);
  } else {
    av1_convolve(src, src_stride, dst, dst_stride, w, h, interp_filter,
                 subpel_x, xs, subpel_y, ys, ref);
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
static INLINE void high_inter_predictor(const uint8_t *src, int src_stride,
                                        uint8_t *dst, int dst_stride,
                                        const int subpel_x, const int subpel_y,
                                        const struct scale_factors *sf, int w,
                                        int h, int ref,
                                        const InterpFilter *interp_filter,
                                        int xs, int ys, int bd) {
  InterpFilterParams interp_filter_params =
      get_interp_filter_params(*interp_filter);
  if (interp_filter_params.taps == SUBPEL_TAPS) {
    const int16_t *filter_x =
        get_interp_filter_subpel_kernel(interp_filter_params, subpel_x);
    const int16_t *filter_y =
        get_interp_filter_subpel_kernel(interp_filter_params, subpel_y);

    sf->highbd_predict[subpel_x != 0][subpel_y != 0][ref](
        src, src_stride, dst, dst_stride, filter_x, xs, filter_y, ys, w, h, bd);
  } else {
    av1_highbd_convolve(src, src_stride, dst, dst_stride, w, h, interp_filter,
                        subpel_x, xs, subpel_y, ys, ref, bd);
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

static INLINE int round_mv_comp_q4(int value) {
  return (value < 0 ? value - 2 : value + 2) / 4;
}

static MV mi_mv_pred_q4(const MODE_INFO *mi, int idx) {
  MV res = {
    round_mv_comp_q4(
        mi->bmi[0].as_mv[idx].as_mv.row + mi->bmi[1].as_mv[idx].as_mv.row +
        mi->bmi[2].as_mv[idx].as_mv.row + mi->bmi[3].as_mv[idx].as_mv.row),
    round_mv_comp_q4(
        mi->bmi[0].as_mv[idx].as_mv.col + mi->bmi[1].as_mv[idx].as_mv.col +
        mi->bmi[2].as_mv[idx].as_mv.col + mi->bmi[3].as_mv[idx].as_mv.col)
  };
  return res;
}

static INLINE int round_mv_comp_q2(int value) {
  return (value < 0 ? value - 1 : value + 1) / 2;
}

static MV mi_mv_pred_q2(const MODE_INFO *mi, int idx, int block0, int block1) {
  MV res = { round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.row +
                              mi->bmi[block1].as_mv[idx].as_mv.row),
             round_mv_comp_q2(mi->bmi[block0].as_mv[idx].as_mv.col +
                              mi->bmi[block1].as_mv[idx].as_mv.col) };
  return res;
}

// TODO(jkoleszar): yet another mv clamping function :-(
static INLINE MV clamp_mv_to_umv_border_sb(const MACROBLOCKD *xd,
                                           const MV *src_mv, int bw, int bh,
                                           int ss_x, int ss_y) {
  // If the MV points so far into the UMV border that no visible pixels
  // are used for reconstruction, the subpel part of the MV can be
  // discarded and the MV limited to 16 pixels with equivalent results.
  const int spel_left = (AOM_INTERP_EXTEND + bw) << SUBPEL_BITS;
  const int spel_right = spel_left - SUBPEL_SHIFTS;
  const int spel_top = (AOM_INTERP_EXTEND + bh) << SUBPEL_BITS;
  const int spel_bottom = spel_top - SUBPEL_SHIFTS;
  MV clamped_mv = { src_mv->row * (1 << (1 - ss_y)),
                    src_mv->col * (1 << (1 - ss_x)) };
  assert(ss_x <= 1);
  assert(ss_y <= 1);

  clamp_mv(&clamped_mv, xd->mb_to_left_edge * (1 << (1 - ss_x)) - spel_left,
           xd->mb_to_right_edge * (1 << (1 - ss_x)) + spel_right,
           xd->mb_to_top_edge * (1 << (1 - ss_y)) - spel_top,
           xd->mb_to_bottom_edge * (1 << (1 - ss_y)) + spel_bottom);

  return clamped_mv;
}

static INLINE MV average_split_mvs(const struct macroblockd_plane *pd,
                                   const MODE_INFO *mi, int ref, int block) {
  const int ss_idx = ((pd->subsampling_x > 0) << 1) | (pd->subsampling_y > 0);
  MV res = { 0, 0 };
  switch (ss_idx) {
    case 0: res = mi->bmi[block].as_mv[ref].as_mv; break;
    case 1: res = mi_mv_pred_q2(mi, ref, block, block + 2); break;
    case 2: res = mi_mv_pred_q2(mi, ref, block, block + 1); break;
    case 3: res = mi_mv_pred_q4(mi, ref); break;
    default: assert(ss_idx <= 3 && ss_idx >= 0);
  }
  return res;
}

void build_inter_predictors(MACROBLOCKD *xd, int plane,
#if CONFIG_MOTION_VAR
                            int mi_col_offset, int mi_row_offset,
#endif  // CONFIG_MOTION_VAR
                            int block, int bw, int bh, int x, int y, int w,
                            int h, int mi_x, int mi_y);

void av1_build_inter_predictor_sub8x8(MACROBLOCKD *xd, int plane, int i, int ir,
                                      int ic, int mi_row, int mi_col);

void av1_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize);

void av1_build_inter_predictors_sbp(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize, int plane);

void av1_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BLOCK_SIZE bsize);

void av1_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize);

void av1_build_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                               int dst_stride, const MV *mv_q3,
                               const struct scale_factors *sf, int w, int h,
                               int do_avg, const InterpFilter *interp_filter,
                               enum mv_precision precision, int x, int y);

#if CONFIG_AOM_HIGHBITDEPTH
void av1_highbd_build_inter_predictor(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const MV *mv_q3, const struct scale_factors *sf, int w, int h, int do_avg,
    const InterpFilter *interp_filter, enum mv_precision precision, int x,
    int y, int bd);
#endif

static INLINE int scaled_buffer_offset(int x_offset, int y_offset, int stride,
                                       const struct scale_factors *sf) {
  const int x = sf ? sf->scale_value_x(x_offset, sf) : x_offset;
  const int y = sf ? sf->scale_value_y(y_offset, sf) : y_offset;
  return y * stride + x;
}

static INLINE void setup_pred_plane(struct buf_2d *dst, uint8_t *src,
                                    int stride, int mi_row, int mi_col,
                                    const struct scale_factors *scale,
                                    int subsampling_x, int subsampling_y) {
  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->stride = stride;
}

void av1_setup_dst_planes(struct macroblockd_plane planes[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src, int mi_row,
                          int mi_col);

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf);

#if CONFIG_MOTION_VAR
void av1_setup_obmc_mask(int length, const uint8_t *mask[2]);
void av1_build_obmc_inter_prediction(AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col,
                                     int use_tmp_dst_buf,
                                     uint8_t *final_buf[MAX_MB_PLANE],
                                     const int final_stride[MAX_MB_PLANE],
                                     uint8_t *above_pred_buf[MAX_MB_PLANE],
                                     const int above_pred_stride[MAX_MB_PLANE],
                                     uint8_t *left_pred_buf[MAX_MB_PLANE],
                                     const int left_pred_stride[MAX_MB_PLANE]);
void av1_build_prediction_by_above_preds(AV1_COMMON *cm, MACROBLOCKD *xd,
                                         int mi_row, int mi_col,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         const int tmp_stride[MAX_MB_PLANE]);
void av1_build_prediction_by_left_preds(AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        const int tmp_stride[MAX_MB_PLANE]);
void av1_build_obmc_inter_predictors_sb(AV1_COMMON *cm, MACROBLOCKD *xd,
                                        int mi_row, int mi_col);
#endif  // CONFIG_MOTION_VAR
static INLINE int has_subpel_mv_component(const MODE_INFO *const mi,
                                          const MACROBLOCKD *const xd,
                                          int dir) {
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int ref = (dir >> 1);

  if (bsize >= BLOCK_8X8) {
    if (dir & 0x01) {
      if (mbmi->mv[ref].as_mv.col & SUBPEL_MASK) return 1;
    } else {
      if (mbmi->mv[ref].as_mv.row & SUBPEL_MASK) return 1;
    }
  } else {
    int plane;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      const PARTITION_TYPE bp = BLOCK_8X8 - bsize;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int have_vsplit = bp != PARTITION_HORZ;
      const int have_hsplit = bp != PARTITION_VERT;
      const int num_4x4_w = 2 >> ((!have_vsplit) | pd->subsampling_x);
      const int num_4x4_h = 2 >> ((!have_hsplit) | pd->subsampling_y);

      int x, y;
      for (y = 0; y < num_4x4_h; ++y) {
        for (x = 0; x < num_4x4_w; ++x) {
          const MV mv = average_split_mvs(pd, mi, ref, y * 2 + x);
          if (dir & 0x01) {
            if (mv.col & SUBPEL_MASK) return 1;
          } else {
            if (mv.row & SUBPEL_MASK) return 1;
          }
        }
      }
    }
  }
  return 0;
}

static INLINE int is_interp_needed(const MACROBLOCKD *const xd) {
  MODE_INFO *const mi = xd->mi[0];
  const int is_compound = has_second_ref(&mi->mbmi);
  int ref;
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    int row_col;
    for (row_col = 0; row_col < 2; ++row_col) {
      const int dir = (ref << 1) + row_col;
      if (has_subpel_mv_component(mi, xd, dir)) {
        return 1;
      }
    }
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RECONINTER_H_
