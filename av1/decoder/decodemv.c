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

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"

#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"

#include "aom_dsp/aom_dsp_common.h"

static PREDICTION_MODE read_intra_mode(aom_reader *r, const aom_prob *p) {
  return (PREDICTION_MODE)aom_read_tree(r, av1_intra_mode_tree, p);
}

static PREDICTION_MODE read_intra_mode_y(AV1_COMMON *cm, MACROBLOCKD *xd,
                                         aom_reader *r, int size_group) {
  const PREDICTION_MODE y_mode =
      read_intra_mode(r, cm->fc->y_mode_prob[size_group]);
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->y_mode[size_group][y_mode];
  return y_mode;
}

static PREDICTION_MODE read_intra_mode_uv(AV1_COMMON *cm, MACROBLOCKD *xd,
                                          aom_reader *r,
                                          PREDICTION_MODE y_mode) {
  const PREDICTION_MODE uv_mode =
      read_intra_mode(r, cm->fc->uv_mode_prob[y_mode]);
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->uv_mode[y_mode][uv_mode];
  return uv_mode;
}

static PREDICTION_MODE read_inter_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                       aom_reader *r, int16_t ctx) {
#if CONFIG_REF_MV
  FRAME_COUNTS *counts = xd->counts;
  int16_t mode_ctx = ctx & NEWMV_CTX_MASK;
  aom_prob mode_prob = cm->fc->newmv_prob[mode_ctx];

  if (aom_read(r, mode_prob) == 0) {
    if (counts) ++counts->newmv_mode[mode_ctx][0];
    return NEWMV;
  }
  if (counts) ++counts->newmv_mode[mode_ctx][1];

  if (ctx & (1 << ALL_ZERO_FLAG_OFFSET)) return ZEROMV;

  mode_ctx = (ctx >> ZEROMV_OFFSET) & ZEROMV_CTX_MASK;

  mode_prob = cm->fc->zeromv_prob[mode_ctx];
  if (aom_read(r, mode_prob) == 0) {
    if (counts) ++counts->zeromv_mode[mode_ctx][0];
    return ZEROMV;
  }
  if (counts) ++counts->zeromv_mode[mode_ctx][1];

  mode_ctx = (ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;

  if (ctx & (1 << SKIP_NEARESTMV_OFFSET)) mode_ctx = 6;
  if (ctx & (1 << SKIP_NEARMV_OFFSET)) mode_ctx = 7;
  if (ctx & (1 << SKIP_NEARESTMV_SUB8X8_OFFSET)) mode_ctx = 8;

  mode_prob = cm->fc->refmv_prob[mode_ctx];
  if (aom_read(r, mode_prob) == 0) {
    if (counts) ++counts->refmv_mode[mode_ctx][0];
    return NEARESTMV;
  } else {
    if (counts) ++counts->refmv_mode[mode_ctx][1];
    return NEARMV;
  }

  // Invalid prediction mode.
  assert(0);
#else
  const int mode =
      aom_read_tree(r, av1_inter_mode_tree, cm->fc->inter_mode_probs[ctx]);
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->inter_mode[ctx][mode];

  return NEARESTMV + mode;
#endif
}

#if CONFIG_REF_MV
static void read_drl_idx(const AV1_COMMON *cm, MACROBLOCKD *xd,
                         MB_MODE_INFO *mbmi, aom_reader *r) {
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
  mbmi->ref_mv_idx = 0;

  if (mbmi->mode == NEWMV) {
    int idx;
    for (idx = 0; idx < 2; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
        aom_prob drl_prob = cm->fc->drl_prob[drl_ctx];
        if (!aom_read(r, drl_prob)) {
          mbmi->ref_mv_idx = idx;
          if (xd->counts) ++xd->counts->drl_mode[drl_ctx][0];
          return;
        }
        mbmi->ref_mv_idx = idx + 1;
        if (xd->counts) ++xd->counts->drl_mode[drl_ctx][1];
      }
    }
  }

  if (mbmi->mode == NEARMV) {
    int idx;
    // Offset the NEARESTMV mode.
    // TODO(jingning): Unify the two syntax decoding loops after the NEARESTMV
    // mode is factored in.
    for (idx = 1; idx < 3; ++idx) {
      if (xd->ref_mv_count[ref_frame_type] > idx + 1) {
        uint8_t drl_ctx = av1_drl_ctx(xd->ref_mv_stack[ref_frame_type], idx);
        aom_prob drl_prob = cm->fc->drl_prob[drl_ctx];
        if (!aom_read(r, drl_prob)) {
          mbmi->ref_mv_idx = idx - 1;
          if (xd->counts) ++xd->counts->drl_mode[drl_ctx][0];
          return;
        }
        mbmi->ref_mv_idx = idx;
        if (xd->counts) ++xd->counts->drl_mode[drl_ctx][1];
      }
    }
  }
}
#endif

#if CONFIG_MOTION_VAR
static MOTION_MODE read_motion_mode(AV1_COMMON *cm, MACROBLOCKD *xd,
                                    MB_MODE_INFO *mbmi, aom_reader *r) {
  if (is_motion_variation_allowed(mbmi)) {
    int motion_mode;
    FRAME_COUNTS *counts = xd->counts;

    motion_mode = aom_read_tree(r, av1_motion_mode_tree,
                                cm->fc->motion_mode_prob[mbmi->sb_type]);
    if (counts) ++counts->motion_mode[mbmi->sb_type][motion_mode];
    return (MOTION_MODE)(SIMPLE_TRANSLATION + motion_mode);
  } else {
    return SIMPLE_TRANSLATION;
  }
}
#endif  // CONFIG_MOTION_VAR

static int read_segment_id(aom_reader *r,
                           const struct segmentation_probs *segp) {
  return aom_read_tree(r, av1_segment_tree, segp->tree_probs);
}

static TX_SIZE read_selected_tx_size(AV1_COMMON *cm, MACROBLOCKD *xd,
                                     TX_SIZE max_tx_size, aom_reader *r) {
  FRAME_COUNTS *counts = xd->counts;
  const int ctx = get_tx_size_context(xd);
  const aom_prob *tx_probs = get_tx_probs(max_tx_size, ctx, &cm->fc->tx_probs);
  int tx_size = aom_read(r, tx_probs[0]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    tx_size += aom_read(r, tx_probs[1]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      tx_size += aom_read(r, tx_probs[2]);
  }

  if (counts) ++get_tx_counts(max_tx_size, ctx, &counts->tx)[tx_size];
  return (TX_SIZE)tx_size;
}

static TX_SIZE read_tx_size(AV1_COMMON *cm, MACROBLOCKD *xd, int allow_select,
                            aom_reader *r) {
  TX_MODE tx_mode = cm->tx_mode;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  if (xd->lossless[xd->mi[0]->mbmi.segment_id]) return TX_4X4;
  if (allow_select && tx_mode == TX_MODE_SELECT && bsize >= BLOCK_8X8)
    return read_selected_tx_size(cm, xd, max_tx_size, r);
  else
    return AOMMIN(max_tx_size, tx_mode_to_biggest_tx_size[tx_mode]);
}

static int dec_get_segment_id(const AV1_COMMON *cm, const uint8_t *segment_ids,
                              int mi_offset, int x_mis, int y_mis) {
  int x, y, segment_id = INT_MAX;

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      segment_id =
          AOMMIN(segment_id, segment_ids[mi_offset + y * cm->mi_cols + x]);

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);
  return segment_id;
}

static void set_segment_id(AV1_COMMON *cm, int mi_offset, int x_mis, int y_mis,
                           int segment_id) {
  int x, y;

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      cm->current_frame_seg_map[mi_offset + y * cm->mi_cols + x] = segment_id;
}

static int read_intra_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_offset, int x_mis, int y_mis,
                                 aom_reader *r) {
  struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  FRAME_COUNTS *counts = xd->counts;
  struct segmentation_probs *const segp = &cm->fc->seg;
#else
  struct segmentation_probs *const segp = &cm->segp;
#endif
  int segment_id;

#if !CONFIG_MISC_FIXES
  (void)xd;
#endif

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  assert(seg->update_map && !seg->temporal_update);

  segment_id = read_segment_id(r, segp);
#if CONFIG_MISC_FIXES
  if (counts) ++counts->seg.tree_total[segment_id];
#endif
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static void copy_segment_id(const AV1_COMMON *cm,
                            const uint8_t *last_segment_ids,
                            uint8_t *current_segment_ids, int mi_offset,
                            int x_mis, int y_mis) {
  int x, y;

  for (y = 0; y < y_mis; y++)
    for (x = 0; x < x_mis; x++)
      current_segment_ids[mi_offset + y * cm->mi_cols + x] =
          last_segment_ids ? last_segment_ids[mi_offset + y * cm->mi_cols + x]
                           : 0;
}

static int read_inter_segment_id(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                 int mi_row, int mi_col, aom_reader *r) {
  struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  FRAME_COUNTS *counts = xd->counts;
  struct segmentation_probs *const segp = &cm->fc->seg;
#else
  struct segmentation_probs *const segp = &cm->segp;
#endif
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int predicted_segment_id, segment_id;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = xd->plane[0].n4_w >> 1;
  const int bh = xd->plane[0].n4_h >> 1;

  // TODO(slavarnway): move x_mis, y_mis into xd ?????
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);

  if (!seg->enabled) return 0;  // Default for disabled segmentation

  predicted_segment_id = cm->last_frame_seg_map
                             ? dec_get_segment_id(cm, cm->last_frame_seg_map,
                                                  mi_offset, x_mis, y_mis)
                             : 0;

  if (!seg->update_map) {
    copy_segment_id(cm, cm->last_frame_seg_map, cm->current_frame_seg_map,
                    mi_offset, x_mis, y_mis);
    return predicted_segment_id;
  }

  if (seg->temporal_update) {
    const int ctx = av1_get_pred_context_seg_id(xd);
    const aom_prob pred_prob = segp->pred_probs[ctx];
    mbmi->seg_id_predicted = aom_read(r, pred_prob);
#if CONFIG_MISC_FIXES
    if (counts) ++counts->seg.pred[ctx][mbmi->seg_id_predicted];
#endif
    if (mbmi->seg_id_predicted) {
      segment_id = predicted_segment_id;
    } else {
      segment_id = read_segment_id(r, segp);
#if CONFIG_MISC_FIXES
      if (counts) ++counts->seg.tree_mispred[segment_id];
#endif
    }
  } else {
    segment_id = read_segment_id(r, segp);
#if CONFIG_MISC_FIXES
    if (counts) ++counts->seg.tree_total[segment_id];
#endif
  }
  set_segment_id(cm, mi_offset, x_mis, y_mis, segment_id);
  return segment_id;
}

static int read_skip(AV1_COMMON *cm, const MACROBLOCKD *xd, int segment_id,
                     aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int ctx = av1_get_skip_context(xd);
    const int skip = aom_read(r, cm->fc->skip_probs[ctx]);
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->skip[ctx][skip];
    return skip;
  }
}

#if CONFIG_EXT_INTRA
static INLINE int read_uniform(aom_reader *r, int n) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  const int v = aom_read_literal(r, l - 1);

  assert(l != 0);
  return (v < m) ? v : ((v << 1) - m + aom_read_literal(r, 1));
}

static void read_intra_angle_info(MB_MODE_INFO *const mbmi, aom_reader *r) {
  mbmi->intra_angle_delta[0] = 0;
  mbmi->intra_angle_delta[1] = 0;
  if (mbmi->sb_type < BLOCK_8X8) return;

  if (is_directional_mode(mbmi->mode)) {
    const TX_SIZE max_tx_size = max_txsize_lookup[mbmi->sb_type];
    const int max_angle_delta = av1_max_angle_delta_y[max_tx_size][mbmi->mode];
    mbmi->intra_angle_delta[0] =
        read_uniform(r, 2 * max_angle_delta + 1) - max_angle_delta;
  }

  if (is_directional_mode(mbmi->uv_mode)) {
    mbmi->intra_angle_delta[1] =
        read_uniform(r, 2 * MAX_ANGLE_DELTA_UV + 1) - MAX_ANGLE_DELTA_UV;
  }
}
#endif  // CONFIG_EXT_INTRA

static void read_intra_frame_mode_info(AV1_COMMON *const cm,
                                       MACROBLOCKD *const xd, int mi_row,
                                       int mi_col, aom_reader *r) {
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  int i;
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = xd->plane[0].n4_w >> 1;
  const int bh = xd->plane[0].n4_h >> 1;

  // TODO(slavarnway): move x_mis, y_mis into xd ?????
  const int x_mis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int y_mis = AOMMIN(cm->mi_rows - mi_row, bh);

  mbmi->segment_id = read_intra_segment_id(cm, xd, mi_offset, x_mis, y_mis, r);
  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);
  mbmi->tx_size = read_tx_size(cm, xd, 1, r);
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE;

  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode =
            read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, i));
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode =
          read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, 1));
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode =
          read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, 2));
      break;
    default:
      mbmi->mode =
          read_intra_mode(r, get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
  }

  mbmi->uv_mode = read_intra_mode_uv(cm, xd, r, mbmi->mode);
#if CONFIG_EXT_INTRA
  read_intra_angle_info(mbmi, r);
#endif  // CONFIG_EXT_INTRA

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    FRAME_COUNTS *counts = xd->counts;
    TX_TYPE tx_type_nom = intra_mode_to_tx_type_context[mbmi->mode];
    mbmi->tx_type =
        aom_read_tree(r, av1_ext_tx_tree,
                      cm->fc->intra_ext_tx_prob[mbmi->tx_size][tx_type_nom]);
    if (counts)
      ++counts->intra_ext_tx[mbmi->tx_size][tx_type_nom][mbmi->tx_type];
  } else {
    mbmi->tx_type = DCT_DCT;
  }
}

static int read_mv_component(aom_reader *r, const nmv_component *mvcomp,
                             int usehp) {
  int mag, d, fr, hp;
  const int sign = aom_read(r, mvcomp->sign);
  const int mv_class = aom_read_tree(r, av1_mv_class_tree, mvcomp->classes);
  const int class0 = mv_class == MV_CLASS_0;

  // Integer part
  if (class0) {
    d = aom_read_tree(r, av1_mv_class0_tree, mvcomp->class0);
    mag = 0;
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits

    d = 0;
    for (i = 0; i < n; ++i) d |= aom_read(r, mvcomp->bits[i]) << i;
    mag = CLASS0_SIZE << (mv_class + 2);
  }

  // Fractional part
  fr = aom_read_tree(r, av1_mv_fp_tree,
                     class0 ? mvcomp->class0_fp[d] : mvcomp->fp);

  // High precision part (if hp is not used, the default value of the hp is 1)
  hp = usehp ? aom_read(r, class0 ? mvcomp->class0_hp : mvcomp->hp) : 1;

  // Result
  mag += ((d << 3) | (fr << 1) | hp) + 1;
  return sign ? -mag : mag;
}

static INLINE void read_mv(aom_reader *r, MV *mv, const MV *ref,
                           const nmv_context *ctx, nmv_context_counts *counts,
                           int allow_hp) {
  const MV_JOINT_TYPE joint_type =
      (MV_JOINT_TYPE)aom_read_tree(r, av1_mv_joint_tree, ctx->joints);
  const int use_hp = allow_hp && av1_use_mv_hp(ref);
  MV diff = { 0, 0 };

  if (mv_joint_vertical(joint_type))
    diff.row = read_mv_component(r, &ctx->comps[0], use_hp);

  if (mv_joint_horizontal(joint_type))
    diff.col = read_mv_component(r, &ctx->comps[1], use_hp);

  av1_inc_mv(&diff, counts, use_hp);

  mv->row = ref->row + diff.row;
  mv->col = ref->col + diff.col;
}

static REFERENCE_MODE read_block_reference_mode(AV1_COMMON *cm,
                                                const MACROBLOCKD *xd,
                                                aom_reader *r) {
  if (cm->reference_mode == REFERENCE_MODE_SELECT) {
    const int ctx = av1_get_reference_mode_context(cm, xd);
    const REFERENCE_MODE mode =
        (REFERENCE_MODE)aom_read(r, cm->fc->comp_inter_prob[ctx]);
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->comp_inter[ctx][mode];
    return mode;  // SINGLE_REFERENCE or COMPOUND_REFERENCE
  } else {
    return cm->reference_mode;
  }
}

// Read the referncence frame
static void read_ref_frames(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                            aom_reader *r, int segment_id,
                            MV_REFERENCE_FRAME ref_frame[2]) {
  FRAME_CONTEXT *const fc = cm->fc;
  FRAME_COUNTS *counts = xd->counts;

  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    ref_frame[0] = (MV_REFERENCE_FRAME)get_segdata(&cm->seg, segment_id,
                                                   SEG_LVL_REF_FRAME);
    ref_frame[1] = NONE;
  } else {
    const REFERENCE_MODE mode = read_block_reference_mode(cm, xd, r);
    // FIXME(rbultje) I'm pretty sure this breaks segmentation ref frame coding
    if (mode == COMPOUND_REFERENCE) {
#if CONFIG_EXT_REFS
      const int idx = cm->ref_frame_sign_bias[cm->comp_bwd_ref[0]];
      const int ctx_fwd = av1_get_pred_context_comp_fwdref_p(cm, xd);
      const int bit_fwd = aom_read(r, fc->comp_fwdref_prob[ctx_fwd][0]);
      const int ctx_bwd = av1_get_pred_context_comp_bwdref_p(cm, xd);
      const int bit_bwd = aom_read(r, fc->comp_bwdref_prob[ctx_bwd][0]);

      // Decode forward references.
      if (counts) ++counts->comp_fwdref[ctx_fwd][0][bit_fwd];
      if (!bit_fwd) {
        const int ctx_fwd1 = av1_get_pred_context_comp_fwdref_p1(cm, xd);
        const int bit_fwd1 = aom_read(r, fc->comp_fwdref_prob[ctx_fwd1][1]);
        if (counts) ++counts->comp_fwdref[ctx_fwd1][1][bit_fwd1];
        ref_frame[!idx] = cm->comp_fwd_ref[bit_fwd1 ? 0 : 1];
      } else {
        const int ctx_fwd2 = av1_get_pred_context_comp_fwdref_p2(cm, xd);
        const int bit_fwd2 = aom_read(r, fc->comp_fwdref_prob[ctx_fwd2][2]);
        if (counts) ++counts->comp_fwdref[ctx_fwd2][2][bit_fwd2];
        ref_frame[!idx] = cm->comp_fwd_ref[bit_fwd2 ? 3 : 2];
      }

      // Decode backward references.
      if (counts) ++counts->comp_bwdref[ctx_bwd][0][bit_bwd];
      ref_frame[idx] = cm->comp_bwd_ref[bit_bwd];
#else
      const int idx = cm->ref_frame_sign_bias[cm->comp_fixed_ref];
      const int ctx = av1_get_pred_context_comp_ref_p(cm, xd);
      const int bit = aom_read(r, fc->comp_ref_prob[ctx]);

      if (counts) ++counts->comp_ref[ctx][bit];
      ref_frame[idx] = cm->comp_fixed_ref;
      ref_frame[!idx] = cm->comp_var_ref[bit];
#endif  // CONFIG_EXT_REFS
    } else if (mode == SINGLE_REFERENCE) {
#if CONFIG_EXT_REFS
      const int ctx0 = av1_get_pred_context_single_ref_p1(xd);
      const int bit0 = aom_read(r, fc->single_ref_prob[ctx0][0]);
      if (counts) ++counts->single_ref[ctx0][0][bit0];
      if (bit0) {
        const int ctx1 = av1_get_pred_context_single_ref_p2(xd);
        const int bit1 = aom_read(r, fc->single_ref_prob[ctx1][1]);
        if (counts) ++counts->single_ref[ctx1][1][bit1];
        ref_frame[0] = bit1 ? ALTREF_FRAME : BWDREF_FRAME;
      } else {
        const int ctx2 = av1_get_pred_context_single_ref_p3(xd);
        const int bit2 = aom_read(r, fc->single_ref_prob[ctx2][2]);
        if (counts) ++counts->single_ref[ctx2][2][bit2];
        if (bit2) {
          const int ctx4 = av1_get_pred_context_single_ref_p5(xd);
          const int bit4 = aom_read(r, fc->single_ref_prob[ctx4][4]);
          if (counts) ++counts->single_ref[ctx4][4][bit4];
          ref_frame[0] = bit4 ? GOLDEN_FRAME : LAST3_FRAME;
        } else {
          const int ctx3 = av1_get_pred_context_single_ref_p4(xd);
          const int bit3 = aom_read(r, fc->single_ref_prob[ctx3][3]);
          if (counts) ++counts->single_ref[ctx3][3][bit3];
          ref_frame[0] = bit3 ? LAST2_FRAME : LAST_FRAME;
        }
      }
#else
      const int ctx0 = av1_get_pred_context_single_ref_p1(xd);
      const int bit0 = aom_read(r, fc->single_ref_prob[ctx0][0]);
      if (counts) ++counts->single_ref[ctx0][0][bit0];
      if (bit0) {
        const int ctx1 = av1_get_pred_context_single_ref_p2(xd);
        const int bit1 = aom_read(r, fc->single_ref_prob[ctx1][1]);
        if (counts) ++counts->single_ref[ctx1][1][bit1];
        ref_frame[0] = bit1 ? ALTREF_FRAME : GOLDEN_FRAME;
      } else {
        ref_frame[0] = LAST_FRAME;
      }
#endif  // CONFIG_EXT_REFS

      ref_frame[1] = NONE;
    } else {
      assert(0 && "Invalid prediction mode.");
    }
  }
}

static INLINE InterpFilter read_switchable_interp_filter(AV1_COMMON *const cm,
                                                         MACROBLOCKD *const xd,
                                                         aom_reader *r) {
  const int ctx = av1_get_pred_context_switchable_interp(xd);
  const InterpFilter type = (InterpFilter)aom_read_tree(
      r, av1_switchable_interp_tree, cm->fc->switchable_interp_prob[ctx]);
  FRAME_COUNTS *counts = xd->counts;
  if (counts) ++counts->switchable_interp[ctx][type];
  return type;
}

static void read_intra_block_mode_info(AV1_COMMON *const cm,
                                       MACROBLOCKD *const xd, MODE_INFO *mi,
                                       aom_reader *r) {
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;
  int i;

  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE;

  switch (bsize) {
    case BLOCK_4X4:
      for (i = 0; i < 4; ++i)
        mi->bmi[i].as_mode = read_intra_mode_y(cm, xd, r, 0);
      mbmi->mode = mi->bmi[3].as_mode;
      break;
    case BLOCK_4X8:
      mi->bmi[0].as_mode = mi->bmi[2].as_mode = read_intra_mode_y(cm, xd, r, 0);
      mi->bmi[1].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode_y(cm, xd, r, 0);
      break;
    case BLOCK_8X4:
      mi->bmi[0].as_mode = mi->bmi[1].as_mode = read_intra_mode_y(cm, xd, r, 0);
      mi->bmi[2].as_mode = mi->bmi[3].as_mode = mbmi->mode =
          read_intra_mode_y(cm, xd, r, 0);
      break;
    default:
      mbmi->mode = read_intra_mode_y(cm, xd, r, size_group_lookup[bsize]);
  }

  mbmi->uv_mode = read_intra_mode_uv(cm, xd, r, mbmi->mode);
#if CONFIG_EXT_INTRA
  read_intra_angle_info(mbmi, r);
#endif  // CONFIG_EXT_INTRA
}

static INLINE int is_mv_valid(const MV *mv) {
  return mv->row > MV_LOW && mv->row < MV_UPP && mv->col > MV_LOW &&
         mv->col < MV_UPP;
}

static INLINE int assign_mv(AV1_COMMON *cm, MACROBLOCKD *xd,
                            PREDICTION_MODE mode, int block, int_mv mv[2],
                            int_mv ref_mv[2], int_mv nearest_mv[2],
                            int_mv near_mv[2], int is_compound, int allow_hp,
                            aom_reader *r) {
  int i;
  int ret = 1;

#if CONFIG_REF_MV
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  int_mv *pred_mv =
      (bsize >= BLOCK_8X8) ? mbmi->pred_mv : xd->mi[0]->bmi[block].pred_mv;
#else
  (void)block;
#endif

  switch (mode) {
    case NEWMV: {
      FRAME_COUNTS *counts = xd->counts;
#if !CONFIG_REF_MV
      nmv_context_counts *const mv_counts = counts ? &counts->mv : NULL;
#endif
      for (i = 0; i < 1 + is_compound; ++i) {
#if CONFIG_REF_MV
        int8_t rf_type = av1_ref_frame_type(mbmi->ref_frame);
        int nmv_ctx =
            av1_nmv_ctx(xd->ref_mv_count[rf_type], xd->ref_mv_stack[rf_type], i,
                        mbmi->ref_mv_idx);
        nmv_context_counts *const mv_counts =
            counts ? &counts->mv[nmv_ctx] : NULL;
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, &cm->fc->nmvc[nmv_ctx],
                mv_counts, allow_hp);
#else
        read_mv(r, &mv[i].as_mv, &ref_mv[i].as_mv, &cm->fc->nmvc, mv_counts,
                allow_hp);
#endif
        ret = ret && is_mv_valid(&mv[i].as_mv);
#if CONFIG_REF_MV
        pred_mv[i].as_int = ref_mv[i].as_int;
#endif
      }
      break;
    }
    case NEARESTMV: {
      mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) mv[1].as_int = nearest_mv[1].as_int;
#if CONFIG_REF_MV
      pred_mv[0].as_int = nearest_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = nearest_mv[1].as_int;
#endif
      break;
    }
    case NEARMV: {
      mv[0].as_int = near_mv[0].as_int;
      if (is_compound) mv[1].as_int = near_mv[1].as_int;
#if CONFIG_REF_MV
      pred_mv[0].as_int = near_mv[0].as_int;
      if (is_compound) pred_mv[1].as_int = near_mv[1].as_int;
#endif
      break;
    }
    case ZEROMV: {
      mv[0].as_int = 0;
      if (is_compound) mv[1].as_int = 0;
#if CONFIG_REF_MV
      pred_mv[0].as_int = 0;
      if (is_compound) pred_mv[1].as_int = 0;
#endif
      break;
    }
    default: { return 0; }
  }
  return ret;
}

static int read_is_inter_block(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                               int segment_id, aom_reader *r) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    return get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME) != INTRA_FRAME;
  } else {
    const int ctx = av1_get_intra_inter_context(xd);
    const int is_inter = aom_read(r, cm->fc->intra_inter_prob[ctx]);
    FRAME_COUNTS *counts = xd->counts;
    if (counts) ++counts->intra_inter[ctx][is_inter];
    return is_inter;
  }
}

static void fpm_sync(void *const data, int mi_row) {
  AV1Decoder *const pbi = (AV1Decoder *)data;
  av1_frameworker_wait(pbi->frame_worker_owner, pbi->common.prev_frame,
                       mi_row << MI_BLOCK_SIZE_LOG2);
}

static void read_inter_block_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd,
                                       MODE_INFO *const mi, int mi_row,
                                       int mi_col, aom_reader *r) {
  AV1_COMMON *const cm = &pbi->common;
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  int_mv nearestmv[2], nearmv[2];
  int_mv ref_mvs[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int ref, is_compound;
  int16_t inter_mode_ctx[MODE_CTX_REF_FRAMES];
  int16_t mode_ctx = 0;
  MV_REFERENCE_FRAME ref_frame;

  read_ref_frames(cm, xd, r, mbmi->segment_id, mbmi->ref_frame);
  is_compound = has_second_ref(mbmi);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    RefBuffer *ref_buf = &cm->frame_refs[frame - LAST_FRAME];

    xd->block_refs[ref] = ref_buf;
    if ((!av1_is_valid_scale(&ref_buf->sf)))
      aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                         "Reference frame has invalid dimensions");
    av1_setup_pre_planes(xd, ref, ref_buf->buf, mi_row, mi_col, &ref_buf->sf);
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    av1_find_mv_refs(cm, xd, mi, ref_frame,
#if CONFIG_REF_MV
                     &xd->ref_mv_count[ref_frame], xd->ref_mv_stack[ref_frame],
#endif
                     ref_mvs[ref_frame], mi_row, mi_col, fpm_sync, (void *)pbi,
                     inter_mode_ctx);
  }

#if CONFIG_REF_MV
  for (; ref_frame < MODE_CTX_REF_FRAMES; ++ref_frame) {
    av1_find_mv_refs(cm, xd, mi, ref_frame, &xd->ref_mv_count[ref_frame],
                     xd->ref_mv_stack[ref_frame], ref_mvs[ref_frame], mi_row,
                     mi_col, fpm_sync, (void *)pbi, inter_mode_ctx);

    if (xd->ref_mv_count[ref_frame] < 2) {
      MV_REFERENCE_FRAME rf[2];
      av1_set_ref_frame(rf, ref_frame);
      for (ref = 0; ref < 2; ++ref) {
        lower_mv_precision(&ref_mvs[rf[ref]][0].as_mv, allow_hp);
        lower_mv_precision(&ref_mvs[rf[ref]][1].as_mv, allow_hp);
      }

      if (ref_mvs[rf[0]][0].as_int != 0 || ref_mvs[rf[0]][1].as_int != 0 ||
          ref_mvs[rf[1]][0].as_int != 0 || ref_mvs[rf[1]][1].as_int != 0)
        inter_mode_ctx[ref_frame] &= ~(1 << ALL_ZERO_FLAG_OFFSET);
    }
  }
  mode_ctx =
      av1_mode_context_analyzer(inter_mode_ctx, mbmi->ref_frame, bsize, -1);
  mbmi->ref_mv_idx = 0;
#else
  mode_ctx = inter_mode_ctx[mbmi->ref_frame[0]];
#endif

  if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    mbmi->mode = ZEROMV;
    if (bsize < BLOCK_8X8) {
      aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                         "Invalid usage of segment feature on small blocks");
      return;
    }
  } else {
    if (bsize >= BLOCK_8X8) {
      mbmi->mode = read_inter_mode(cm, xd, r, mode_ctx);
#if CONFIG_REF_MV
      if (mbmi->mode == NEARMV || mbmi->mode == NEWMV)
        read_drl_idx(cm, xd, mbmi, r);
#endif
    }
  }

  if (bsize < BLOCK_8X8 || mbmi->mode != ZEROMV) {
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      av1_find_best_ref_mvs(allow_hp, ref_mvs[mbmi->ref_frame[ref]],
                            &nearestmv[ref], &nearmv[ref]);
    }
  }

#if CONFIG_REF_MV
  if (mbmi->ref_mv_idx > 0) {
    int_mv cur_mv =
        xd->ref_mv_stack[mbmi->ref_frame[0]][1 + mbmi->ref_mv_idx].this_mv;
    lower_mv_precision(&cur_mv.as_mv, cm->allow_high_precision_mv);
    nearmv[0] = cur_mv;
  }

  if (is_compound && bsize >= BLOCK_8X8 && mbmi->mode != NEWMV &&
      mbmi->mode != ZEROMV) {
    uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);

    if (xd->ref_mv_count[ref_frame_type] == 1 && mbmi->mode == NEARESTMV) {
      int i;
      nearestmv[0] = xd->ref_mv_stack[ref_frame_type][0].this_mv;
      nearestmv[1] = xd->ref_mv_stack[ref_frame_type][0].comp_mv;

      for (i = 0; i < 2; ++i) lower_mv_precision(&nearestmv[i].as_mv, allow_hp);
    }

    if (xd->ref_mv_count[ref_frame_type] > 1) {
      int i;
      const int ref_mv_idx = 1 + mbmi->ref_mv_idx;
      nearestmv[0] = xd->ref_mv_stack[ref_frame_type][0].this_mv;
      nearestmv[1] = xd->ref_mv_stack[ref_frame_type][0].comp_mv;
      nearmv[0] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].this_mv;
      nearmv[1] = xd->ref_mv_stack[ref_frame_type][ref_mv_idx].comp_mv;

      for (i = 0; i < 2; ++i) {
        lower_mv_precision(&nearestmv[i].as_mv, allow_hp);
        lower_mv_precision(&nearmv[i].as_mv, allow_hp);
      }
    }
  }
#endif

  mbmi->interp_filter = (cm->interp_filter == SWITCHABLE)
                            ? read_switchable_interp_filter(cm, xd, r)
                            : cm->interp_filter;

  if (bsize < BLOCK_8X8) {
    const int num_4x4_w = 1 << xd->bmode_blocks_wl;
    const int num_4x4_h = 1 << xd->bmode_blocks_hl;
    int idx, idy;
    PREDICTION_MODE b_mode;
    int_mv nearest_sub8x8[2], near_sub8x8[2];
    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        int_mv block[2];
        const int j = idy * 2 + idx;
#if CONFIG_REF_MV
        mode_ctx = av1_mode_context_analyzer(inter_mode_ctx, mbmi->ref_frame,
                                             bsize, j);
#endif
        b_mode = read_inter_mode(cm, xd, r, mode_ctx);

        if (b_mode == NEARESTMV || b_mode == NEARMV) {
          for (ref = 0; ref < 1 + is_compound; ++ref)
            av1_append_sub8x8_mvs_for_idx(cm, xd, j, ref, mi_row, mi_col,
                                          &nearest_sub8x8[ref],
                                          &near_sub8x8[ref]);
        }

        if (!assign_mv(cm, xd, b_mode, j, block, nearestmv, nearest_sub8x8,
                       near_sub8x8, is_compound, allow_hp, r)) {
          xd->corrupted |= 1;
          break;
        };

        mi->bmi[j].as_mv[0].as_int = block[0].as_int;
        if (is_compound) mi->bmi[j].as_mv[1].as_int = block[1].as_int;

        if (num_4x4_h == 2) mi->bmi[j + 2] = mi->bmi[j];
        if (num_4x4_w == 2) mi->bmi[j + 1] = mi->bmi[j];
      }
    }

    mi->mbmi.mode = b_mode;
#if CONFIG_REF_MV
    mbmi->pred_mv[0].as_int = mi->bmi[3].pred_mv[0].as_int;
    mbmi->pred_mv[1].as_int = mi->bmi[3].pred_mv[1].as_int;
#endif
    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  } else {
#if CONFIG_REF_MV
    int ref;
    for (ref = 0; ref < 1 + is_compound && mbmi->mode == NEWMV; ++ref) {
      int_mv ref_mv = nearestmv[ref];
      uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
      if (xd->ref_mv_count[ref_frame_type] > 1) {
        ref_mv =
            (ref == 0)
                ? xd->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx].this_mv
                : xd->ref_mv_stack[ref_frame_type][mbmi->ref_mv_idx].comp_mv;
        clamp_mv_ref(&ref_mv.as_mv, xd->n8_w << 3, xd->n8_h << 3, xd);
        lower_mv_precision(&ref_mv.as_mv, allow_hp);
      }
      nearestmv[ref] = ref_mv;
    }
#endif
    xd->corrupted |= !assign_mv(cm, xd, mbmi->mode, 0, mbmi->mv, nearestmv,
                                nearestmv, nearmv, is_compound, allow_hp, r);
  }
#if CONFIG_MOTION_VAR
  mbmi->motion_mode = read_motion_mode(cm, xd, mbmi, r);
#endif  // CONFIG_MOTION_VAR
}

static void read_inter_frame_mode_info(AV1Decoder *const pbi,
                                       MACROBLOCKD *const xd, int mi_row,
                                       int mi_col, aom_reader *r) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
  MB_MODE_INFO *const mbmi = &mi->mbmi;
  int inter_block;

  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  mbmi->segment_id = read_inter_segment_id(cm, xd, mi_row, mi_col, r);
  mbmi->skip = read_skip(cm, xd, mbmi->segment_id, r);
  inter_block = read_is_inter_block(cm, xd, mbmi->segment_id, r);
  mbmi->tx_size = read_tx_size(cm, xd, !mbmi->skip || !inter_block, r);

  if (inter_block)
    read_inter_block_mode_info(pbi, xd, mi, mi_row, mi_col, r);
  else
    read_intra_block_mode_info(cm, xd, mi, r);

  if (mbmi->tx_size < TX_32X32 && cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    FRAME_COUNTS *counts = xd->counts;
    if (inter_block) {
      mbmi->tx_type = aom_read_tree(r, av1_ext_tx_tree,
                                    cm->fc->inter_ext_tx_prob[mbmi->tx_size]);
      if (counts) ++counts->inter_ext_tx[mbmi->tx_size][mbmi->tx_type];
    } else {
      const TX_TYPE tx_type_nom = intra_mode_to_tx_type_context[mbmi->mode];
      mbmi->tx_type =
          aom_read_tree(r, av1_ext_tx_tree,
                        cm->fc->intra_ext_tx_prob[mbmi->tx_size][tx_type_nom]);
      if (counts)
        ++counts->intra_ext_tx[mbmi->tx_size][tx_type_nom][mbmi->tx_type];
    }
  } else {
    mbmi->tx_type = DCT_DCT;
  }
}

void av1_read_mode_info(AV1Decoder *const pbi, MACROBLOCKD *xd, int mi_row,
                        int mi_col, aom_reader *r, int x_mis, int y_mis) {
  AV1_COMMON *const cm = &pbi->common;
  MODE_INFO *const mi = xd->mi[0];
  MV_REF *frame_mvs = cm->cur_frame->mvs + mi_row * cm->mi_cols + mi_col;
  int w, h;

  if (frame_is_intra_only(cm)) {
    read_intra_frame_mode_info(cm, xd, mi_row, mi_col, r);
  } else {
    read_inter_frame_mode_info(pbi, xd, mi_row, mi_col, r);

    for (h = 0; h < y_mis; ++h) {
      MV_REF *const frame_mv = frame_mvs + h * cm->mi_cols;
      for (w = 0; w < x_mis; ++w) {
        MV_REF *const mv = frame_mv + w;
        mv->ref_frame[0] = mi->mbmi.ref_frame[0];
        mv->ref_frame[1] = mi->mbmi.ref_frame[1];
        mv->mv[0].as_int = mi->mbmi.mv[0].as_int;
        mv->mv[1].as_int = mi->mbmi.mv[1].as_int;
#if CONFIG_REF_MV
        mv->pred_mv[0].as_int = mi->mbmi.pred_mv[0].as_int;
        mv->pred_mv[1].as_int = mi->mbmi.pred_mv[1].as_int;
#endif
      }
    }
  }
}
