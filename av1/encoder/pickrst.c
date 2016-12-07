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
#include <float.h>
#include <limits.h>
#include <math.h>

#include "./aom_scale_rtcd.h"

#include "aom_dsp/psnr.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"
#include "av1/encoder/quantize.h"

typedef double (*search_restore_type)(const YV12_BUFFER_CONFIG *src,
                                      AV1_COMP *cpi, int filter_level,
                                      int partial_frame, RestorationInfo *info,
                                      double *best_tile_cost);

// const int frame_level_restore_bits[RESTORE_TYPES] = { 2, 2, 3, 3, 2 };
const int frame_level_restore_bits[RESTORE_TYPES] = { 2, 3, 3, 3, 3, 2 };

static int64_t sse_restoration_tile(const YV12_BUFFER_CONFIG *src,
                                    AV1_COMMON *const cm, int h_start,
                                    int width, int v_start, int height) {
  int64_t filt_err;
#if CONFIG_AOM_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    filt_err = aom_highbd_get_y_sse_part(src, cm->frame_to_show, h_start, width,
                                         v_start, height);
  } else {
    filt_err = aom_get_y_sse_part(src, cm->frame_to_show, h_start, width,
                                  v_start, height);
  }
#else
  filt_err = aom_get_y_sse_part(src, cm->frame_to_show, h_start, width, v_start,
                                height);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  return filt_err;
}

static int64_t try_restoration_tile(const YV12_BUFFER_CONFIG *src,
                                    AV1_COMP *const cpi, RestorationInfo *rsi,
                                    int partial_frame, int tile_idx,
                                    int subtile_idx, int subtile_bits) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;
  int tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  const int ntiles = av1_get_rest_ntiles(cm->width, cm->height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);
  (void)ntiles;

  av1_loop_restoration_frame(cm->frame_to_show, cm, rsi, 1, partial_frame);
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, nhtiles,
                           nvtiles, tile_width, tile_height, cm->width,
                           cm->height, 0, 0, &h_start, &h_end, &v_start,
                           &v_end);
  filt_err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                                  v_end - v_start);

  // Re-instate the unfiltered frame
  aom_yv12_copy_y(&cpi->last_frame_db, cm->frame_to_show);
  return filt_err;
}

static int64_t try_restoration_frame(const YV12_BUFFER_CONFIG *src,
                                     AV1_COMP *const cpi, RestorationInfo *rsi,
                                     int partial_frame) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;
  av1_loop_restoration_frame(cm->frame_to_show, cm, rsi, 1, partial_frame);
#if CONFIG_AOM_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    filt_err = aom_highbd_get_y_sse(src, cm->frame_to_show);
  } else {
    filt_err = aom_get_y_sse(src, cm->frame_to_show);
  }
#else
  filt_err = aom_get_y_sse(src, cm->frame_to_show);
#endif  // CONFIG_AOM_HIGHBITDEPTH

  // Re-instate the unfiltered frame
  aom_yv12_copy_y(&cpi->last_frame_db, cm->frame_to_show);
  return filt_err;
}

static int64_t get_pixel_proj_error(int64_t *src, int width, int height,
                                    int src_stride, int64_t *dgd,
                                    int dgd_stride, int64_t *flt1,
                                    int flt1_stride, int64_t *flt2,
                                    int flt2_stride, int *xqd) {
  int i, j;
  int64_t err = 0;
  int xq[2];
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int64_t s = (int64_t)src[i * src_stride + j];
      const int64_t u = (int64_t)dgd[i * dgd_stride + j];
      const int64_t f1 = (int64_t)flt1[i * flt1_stride + j] - u;
      const int64_t f2 = (int64_t)flt2[i * flt2_stride + j] - u;
      const int64_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int64_t e =
          ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) -
          ROUND_POWER_OF_TWO(s, SGRPROJ_RST_BITS);
      err += e * e;
    }
  }
  return err;
}

static void get_proj_subspace(int64_t *src, int width, int height,
                              int src_stride, int64_t *dgd, int dgd_stride,
                              int64_t *flt1, int flt1_stride, int64_t *flt2,
                              int flt2_stride, int *xq) {
  int i, j;
  double H[2][2] = { { 0, 0 }, { 0, 0 } };
  double C[2] = { 0, 0 };
  double Det;
  double x[2];
  const int size = width * height;

  xq[0] = -(1 << SGRPROJ_PRJ_BITS) / 4;
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0];
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const double u = (double)dgd[i * dgd_stride + j];
      const double s = (double)src[i * src_stride + j] - u;
      const double f1 = (double)flt1[i * flt1_stride + j] - u;
      const double f2 = (double)flt2[i * flt2_stride + j] - u;
      H[0][0] += f1 * f1;
      H[1][1] += f2 * f2;
      H[0][1] += f1 * f2;
      C[0] += f1 * s;
      C[1] += f2 * s;
    }
  }
  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
  Det = (H[0][0] * H[1][1] - H[0][1] * H[1][0]);
  if (Det < 1e-8) return;  // ill-posed, return default values
  x[0] = (H[1][1] * C[0] - H[0][1] * C[1]) / Det;
  x[1] = (H[0][0] * C[1] - H[1][0] * C[0]) / Det;
  xq[0] = (int)rint(x[0] * (1 << SGRPROJ_PRJ_BITS));
  xq[1] = (int)rint(x[1] * (1 << SGRPROJ_PRJ_BITS));
}

void encode_xq(int *xq, int *xqd) {
  xqd[0] = -xq[0];
  xqd[0] = clamp(xqd[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
  xqd[1] = (1 << SGRPROJ_PRJ_BITS) + xqd[0] - xq[1];
  xqd[1] = clamp(xqd[1], SGRPROJ_PRJ_MIN1, SGRPROJ_PRJ_MAX1);
}

static void search_selfguided_restoration(uint8_t *dat8, int width, int height,
                                          int dat_stride, uint8_t *src8,
                                          int src_stride, int bit_depth,
                                          int *eps, int *xqd, void *tmpbuf) {
  int64_t *flt1 = (int64_t *)tmpbuf;
  int64_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  uint8_t *tmpbuf2 = (uint8_t *)(flt2 + RESTORATION_TILEPELS_MAX);
  int64_t srd[RESTORATION_TILEPELS_MAX];
  int64_t dgd[RESTORATION_TILEPELS_MAX];
  int i, j, ep, bestep = 0;
  int64_t err, besterr = -1;
  int exqd[2], bestxqd[2] = { 0, 0 };
  for (ep = 0; ep < SGRPROJ_PARAMS; ep++) {
    int exq[2];
    if (bit_depth > 8) {
      uint16_t *src = CONVERT_TO_SHORTPTR(src8);
      uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
      for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
          flt1[i * width + j] = (int64_t)dat[i * dat_stride + j];
          flt2[i * width + j] = (int64_t)dat[i * dat_stride + j];
          dgd[i * width + j] = (int64_t)dat[i * dat_stride + j]
                               << SGRPROJ_RST_BITS;
          srd[i * width + j] = (int64_t)src[i * src_stride + j]
                               << SGRPROJ_RST_BITS;
        }
      }
    } else {
      uint8_t *src = src8;
      uint8_t *dat = dat8;
      for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
          const int k = i * width + j;
          const int l = i * dat_stride + j;
          flt1[k] = (int64_t)dat[l];
          flt2[k] = (int64_t)dat[l];
          dgd[k] = (int64_t)dat[l] << SGRPROJ_RST_BITS;
          srd[k] = (int64_t)src[i * src_stride + j] << SGRPROJ_RST_BITS;
        }
      }
    }
    av1_selfguided_restoration(flt1, width, height, width, bit_depth,
                               sgr_params[ep].r1, sgr_params[ep].e1, tmpbuf2);
    av1_selfguided_restoration(flt2, width, height, width, bit_depth,
                               sgr_params[ep].r2, sgr_params[ep].e2, tmpbuf2);
    get_proj_subspace(srd, width, height, width, dgd, width, flt1, width, flt2,
                      width, exq);
    encode_xq(exq, exqd);
    err = get_pixel_proj_error(srd, width, height, width, dgd, width, flt1,
                               width, flt2, width, exqd);
    if (besterr == -1 || err < besterr) {
      bestep = ep;
      besterr = err;
      bestxqd[0] = exqd[0];
      bestxqd[1] = exqd[1];
    }
  }
  *eps = bestep;
  xqd[0] = bestxqd[0];
  xqd[1] = bestxqd[1];
}

static double search_sgrproj(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                             int filter_level, int partial_frame,
                             RestorationInfo *info, double *best_tile_cost) {
  SgrprojInfo *sgrproj_info = info->sgrproj_info;
  double err, cost_norestore, cost_sgrproj;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  RestorationInfo rsi;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  uint8_t *tmpbuf = aom_malloc(SGRPROJ_TMPBUF_SIZE);
  const int ntiles = av1_get_rest_ntiles(cm->width, cm->height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);
  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  rsi.frame_restoration_type = RESTORE_SGRPROJ;
  rsi.sgrproj_info =
      (SgrprojInfo *)aom_malloc(sizeof(*rsi.sgrproj_info) * ntiles);
  assert(rsi.sgrproj_info != NULL);

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx)
    rsi.sgrproj_info[tile_idx].level = 0;
  // Compute best Sgrproj filters for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, cm->width, cm->height, 0, 0, &h_start,
                             &h_end, &v_start, &v_end);
    err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                               v_end - v_start);
    // #bits when a tile is not restored
    bits = av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 0);
    cost_norestore = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    best_tile_cost[tile_idx] = DBL_MAX;
    search_selfguided_restoration(
        dgd->y_buffer + v_start * dgd->y_stride + h_start, h_end - h_start,
        v_end - v_start, dgd->y_stride,
        src->y_buffer + v_start * src->y_stride + h_start, src->y_stride,
#if CONFIG_AOM_HIGHBITDEPTH
        cm->bit_depth,
#else
        8,
#endif  // CONFIG_AOM_HIGHBITDEPTH
        &rsi.sgrproj_info[tile_idx].ep, rsi.sgrproj_info[tile_idx].xqd, tmpbuf);
    rsi.sgrproj_info[tile_idx].level = 1;
    err = try_restoration_tile(src, cpi, &rsi, partial_frame, tile_idx, 0, 0);
    bits = SGRPROJ_BITS << AV1_PROB_COST_SHIFT;
    bits += av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 1);
    cost_sgrproj = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    if (cost_sgrproj >= cost_norestore) {
      sgrproj_info[tile_idx].level = 0;
    } else {
      memcpy(&sgrproj_info[tile_idx], &rsi.sgrproj_info[tile_idx],
             sizeof(sgrproj_info[tile_idx]));
      bits = SGRPROJ_BITS << AV1_PROB_COST_SHIFT;
      best_tile_cost[tile_idx] = RDCOST_DBL(
          x->rdmult, x->rddiv,
          (bits + cpi->switchable_restore_cost[RESTORE_SGRPROJ]) >> 4, err);
    }
    rsi.sgrproj_info[tile_idx].level = 0;
  }
  // Cost for Sgrproj filtering
  bits = frame_level_restore_bits[rsi.frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    bits +=
        av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, sgrproj_info[tile_idx].level);
    memcpy(&rsi.sgrproj_info[tile_idx], &sgrproj_info[tile_idx],
           sizeof(sgrproj_info[tile_idx]));
    if (sgrproj_info[tile_idx].level) {
      bits += (SGRPROJ_BITS << AV1_PROB_COST_SHIFT);
    }
  }
  err = try_restoration_frame(src, cpi, &rsi, partial_frame);
  cost_sgrproj = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);

  aom_free(rsi.sgrproj_info);
  aom_free(tmpbuf);

  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_sgrproj;
}

static int64_t compute_sse(uint8_t *dgd, int width, int height, int dgd_stride,
                           uint8_t *src, int src_stride) {
  int64_t sse = 0;
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int diff =
          (int)dgd[i * dgd_stride + j] - (int)src[i * src_stride + j];
      sse += diff * diff;
    }
  }
  return sse;
}

#if CONFIG_AOM_HIGHBITDEPTH
static int64_t compute_sse_highbd(uint16_t *dgd, int width, int height,
                                  int dgd_stride, uint16_t *src,
                                  int src_stride) {
  int64_t sse = 0;
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int diff =
          (int)dgd[i * dgd_stride + j] - (int)src[i * src_stride + j];
      sse += diff * diff;
    }
  }
  return sse;
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

static void search_domaintxfmrf_restoration(uint8_t *dgd8, int width,
                                            int height, int dgd_stride,
                                            uint8_t *src8, int src_stride,
                                            int bit_depth, int *sigma_r) {
  const int first_p_step = 8;
  const int second_p_range = first_p_step >> 1;
  const int second_p_step = 2;
  const int third_p_range = second_p_step >> 1;
  const int third_p_step = 1;
  int i, p, best_p0, best_p = -1;
  int64_t best_sse = INT64_MAX, sse;
  if (bit_depth == 8) {
    uint8_t *tmp = (uint8_t *)aom_malloc(width * height * sizeof(*tmp));
    uint8_t *dgd = dgd8;
    uint8_t *src = src8;
    // First phase
    for (p = first_p_step / 2; p < DOMAINTXFMRF_PARAMS; p += first_p_step) {
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration(tmp, width, height, width, p);
      sse = compute_sse(tmp, width, height, width, src, src_stride);
      if (sse < best_sse || best_p == -1) {
        best_p = p;
        best_sse = sse;
      }
    }
    // Second Phase
    best_p0 = best_p;
    for (p = best_p0 - second_p_range; p <= best_p0 + second_p_range;
         p += second_p_step) {
      if (p < 0 || p == best_p || p >= DOMAINTXFMRF_PARAMS) continue;
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration(tmp, width, height, width, p);
      sse = compute_sse(tmp, width, height, width, src, src_stride);
      if (sse < best_sse) {
        best_p = p;
        best_sse = sse;
      }
    }
    // Third Phase
    best_p0 = best_p;
    for (p = best_p0 - third_p_range; p <= best_p0 + third_p_range;
         p += third_p_step) {
      if (p < 0 || p == best_p || p >= DOMAINTXFMRF_PARAMS) continue;
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration(tmp, width, height, width, p);
      sse = compute_sse(tmp, width, height, width, src, src_stride);
      if (sse < best_sse) {
        best_p = p;
        best_sse = sse;
      }
    }
    aom_free(tmp);
  } else {
#if CONFIG_AOM_HIGHBITDEPTH
    uint16_t *tmp = (uint16_t *)aom_malloc(width * height * sizeof(*tmp));
    uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    // First phase
    for (p = first_p_step / 2; p < DOMAINTXFMRF_PARAMS; p += first_p_step) {
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration_highbd(tmp, width, height, width, p,
                                          bit_depth);
      sse = compute_sse_highbd(tmp, width, height, width, src, src_stride);
      if (sse < best_sse || best_p == -1) {
        best_p = p;
        best_sse = sse;
      }
    }
    // Second Phase
    best_p0 = best_p;
    for (p = best_p0 - second_p_range; p <= best_p0 + second_p_range;
         p += second_p_step) {
      if (p < 0 || p == best_p || p >= DOMAINTXFMRF_PARAMS) continue;
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration_highbd(tmp, width, height, width, p,
                                          bit_depth);
      sse = compute_sse_highbd(tmp, width, height, width, src, src_stride);
      if (sse < best_sse) {
        best_p = p;
        best_sse = sse;
      }
    }
    // Third Phase
    best_p0 = best_p;
    for (p = best_p0 - third_p_range; p <= best_p0 + third_p_range;
         p += third_p_step) {
      if (p < 0 || p == best_p || p >= DOMAINTXFMRF_PARAMS) continue;
      for (i = 0; i < height; ++i) {
        memcpy(&tmp[i * width], &dgd[i * dgd_stride], width * sizeof(dgd[0]));
      }
      av1_domaintxfmrf_restoration_highbd(tmp, width, height, width, p,
                                          bit_depth);
      sse = compute_sse_highbd(tmp, width, height, width, src, src_stride);
      if (sse < best_sse) {
        best_p = p;
        best_sse = sse;
      }
    }
    aom_free(tmp);
#else
    assert(0);
#endif  // CONFIG_AOM_HIGHBITDEPTH
  }
  *sigma_r = best_p;
}

static double search_domaintxfmrf(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                                  int filter_level, int partial_frame,
                                  RestorationInfo *info,
                                  double *best_tile_cost) {
  DomaintxfmrfInfo *domaintxfmrf_info = info->domaintxfmrf_info;
  double err, cost_norestore, cost_domaintxfmrf;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  RestorationInfo rsi;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  const int ntiles = av1_get_rest_ntiles(cm->width, cm->height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);
  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  rsi.frame_restoration_type = RESTORE_DOMAINTXFMRF;
  rsi.domaintxfmrf_info =
      (DomaintxfmrfInfo *)aom_malloc(sizeof(*rsi.domaintxfmrf_info) * ntiles);
  assert(rsi.domaintxfmrf_info != NULL);

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx)
    rsi.domaintxfmrf_info[tile_idx].level = 0;
  // Compute best Domaintxfm filters for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, cm->width, cm->height, 0, 0, &h_start,
                             &h_end, &v_start, &v_end);
    err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                               v_end - v_start);
    // #bits when a tile is not restored
    bits = av1_cost_bit(RESTORE_NONE_DOMAINTXFMRF_PROB, 0);
    cost_norestore = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    best_tile_cost[tile_idx] = DBL_MAX;

    search_domaintxfmrf_restoration(
        dgd->y_buffer + v_start * dgd->y_stride + h_start, h_end - h_start,
        v_end - v_start, dgd->y_stride,
        src->y_buffer + v_start * src->y_stride + h_start, src->y_stride,
#if CONFIG_AOM_HIGHBITDEPTH
        cm->bit_depth,
#else
        8,
#endif  // CONFIG_AOM_HIGHBITDEPTH
        &rsi.domaintxfmrf_info[tile_idx].sigma_r);

    rsi.domaintxfmrf_info[tile_idx].level = 1;
    err = try_restoration_tile(src, cpi, &rsi, partial_frame, tile_idx, 0, 0);
    bits = DOMAINTXFMRF_PARAMS_BITS << AV1_PROB_COST_SHIFT;
    bits += av1_cost_bit(RESTORE_NONE_DOMAINTXFMRF_PROB, 1);
    cost_domaintxfmrf = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    if (cost_domaintxfmrf >= cost_norestore) {
      domaintxfmrf_info[tile_idx].level = 0;
    } else {
      memcpy(&domaintxfmrf_info[tile_idx], &rsi.domaintxfmrf_info[tile_idx],
             sizeof(domaintxfmrf_info[tile_idx]));
      bits = DOMAINTXFMRF_PARAMS_BITS << AV1_PROB_COST_SHIFT;
      best_tile_cost[tile_idx] = RDCOST_DBL(
          x->rdmult, x->rddiv,
          (bits + cpi->switchable_restore_cost[RESTORE_DOMAINTXFMRF]) >> 4,
          err);
    }
    rsi.domaintxfmrf_info[tile_idx].level = 0;
  }
  // Cost for Domaintxfmrf filtering
  bits = frame_level_restore_bits[rsi.frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    bits += av1_cost_bit(RESTORE_NONE_DOMAINTXFMRF_PROB,
                         domaintxfmrf_info[tile_idx].level);
    memcpy(&rsi.domaintxfmrf_info[tile_idx], &domaintxfmrf_info[tile_idx],
           sizeof(domaintxfmrf_info[tile_idx]));
    if (domaintxfmrf_info[tile_idx].level) {
      bits += (DOMAINTXFMRF_PARAMS_BITS << AV1_PROB_COST_SHIFT);
    }
  }
  err = try_restoration_frame(src, cpi, &rsi, partial_frame);
  cost_domaintxfmrf = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);

  aom_free(rsi.domaintxfmrf_info);

  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_domaintxfmrf;
}

static double search_bilateral(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                               int filter_level, int partial_frame,
                               RestorationInfo *info, double *best_tile_cost) {
  BilateralInfo *bilateral_info = info->bilateral_info;
  AV1_COMMON *const cm = &cpi->common;
  int i, tile_idx, subtile_idx;
  int64_t err;
  int bits;
  double cost, best_cost, cost_bilateral, cost_norestore_subtile;
  const int bilateral_level_bits = av1_bilateral_level_bits(&cpi->common);
  const int bilateral_levels = 1 << bilateral_level_bits;
  MACROBLOCK *x = &cpi->td.mb;
  RestorationInfo rsi;
  int tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  const int ntiles = av1_get_rest_ntiles(cm->width, cm->height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);

  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  rsi.frame_restoration_type = RESTORE_BILATERAL;
  rsi.bilateral_info =
      (BilateralInfo *)aom_malloc(sizeof(*rsi.bilateral_info) * ntiles);
  assert(rsi.bilateral_info != NULL);

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx)
    for (subtile_idx = 0; subtile_idx < BILATERAL_SUBTILES; ++subtile_idx)
      bilateral_info[tile_idx].level[subtile_idx] =
          rsi.bilateral_info[tile_idx].level[subtile_idx] = -1;

  // Find best filter for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    for (subtile_idx = 0; subtile_idx < BILATERAL_SUBTILES; ++subtile_idx) {
      av1_get_rest_tile_limits(tile_idx, subtile_idx, BILATERAL_SUBTILE_BITS,
                               nhtiles, nvtiles, tile_width, tile_height,
                               cm->width, cm->height, 0, 0, &h_start, &h_end,
                               &v_start, &v_end);
      err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                                 v_end - v_start);
#if BILATERAL_SUBTILES
      // #bits when a subtile is not restored
      bits = av1_cost_bit(RESTORE_NONE_BILATERAL_PROB, 0);
#else
      bits = 0;
#endif
      cost_norestore_subtile =
          RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
      best_cost = cost_norestore_subtile;

      for (i = 0; i < bilateral_levels; ++i) {
        rsi.bilateral_info[tile_idx].level[subtile_idx] = i;
        err = try_restoration_tile(src, cpi, &rsi, partial_frame, tile_idx,
                                   subtile_idx, BILATERAL_SUBTILE_BITS);
        bits = bilateral_level_bits << AV1_PROB_COST_SHIFT;
        bits += av1_cost_bit(RESTORE_NONE_BILATERAL_PROB, 1);
        cost = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
        if (cost < best_cost) {
          bilateral_info[tile_idx].level[subtile_idx] = i;
          best_cost = cost;
        }
        rsi.bilateral_info[tile_idx].level[subtile_idx] = -1;
      }
    }
    bits = 0;
    for (subtile_idx = 0; subtile_idx < BILATERAL_SUBTILES; ++subtile_idx) {
      rsi.bilateral_info[tile_idx].level[subtile_idx] =
          bilateral_info[tile_idx].level[subtile_idx];
      if (rsi.bilateral_info[tile_idx].level[subtile_idx] >= 0)
        bits += bilateral_level_bits << AV1_PROB_COST_SHIFT;
#if BILATERAL_SUBTILES
      bits +=
          av1_cost_bit(RESTORE_NONE_BILATERAL_PROB,
                       rsi.bilateral_info[tile_idx].level[subtile_idx] >= 0);
#endif
    }
    err = try_restoration_tile(src, cpi, &rsi, partial_frame, tile_idx, 0, 0);
    best_tile_cost[tile_idx] = RDCOST_DBL(
        x->rdmult, x->rddiv,
        (bits + cpi->switchable_restore_cost[RESTORE_BILATERAL]) >> 4, err);
  }
  // Find cost for combined configuration
  bits = frame_level_restore_bits[rsi.frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    for (subtile_idx = 0; subtile_idx < BILATERAL_SUBTILES; ++subtile_idx) {
      rsi.bilateral_info[tile_idx].level[subtile_idx] =
          bilateral_info[tile_idx].level[subtile_idx];
      if (rsi.bilateral_info[tile_idx].level[subtile_idx] >= 0) {
        bits += bilateral_level_bits << AV1_PROB_COST_SHIFT;
      }
#if BILATERAL_SUBTILES
      bits +=
          av1_cost_bit(RESTORE_NONE_BILATERAL_PROB,
                       rsi.bilateral_info[tile_idx].level[subtile_idx] >= 0);
#endif
    }
  }
  err = try_restoration_frame(src, cpi, &rsi, partial_frame);
  cost_bilateral = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);

  aom_free(rsi.bilateral_info);

  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_bilateral;
}

static double find_average(uint8_t *src, int h_start, int h_end, int v_start,
                           int v_end, int stride) {
  uint64_t sum = 0;
  double avg = 0;
  int i, j;
  for (i = v_start; i < v_end; i++)
    for (j = h_start; j < h_end; j++) sum += src[i * stride + j];
  avg = (double)sum / ((v_end - v_start) * (h_end - h_start));
  return avg;
}

static void compute_stats(uint8_t *dgd, uint8_t *src, int h_start, int h_end,
                          int v_start, int v_end, int dgd_stride,
                          int src_stride, double *M, double *H) {
  int i, j, k, l;
  double Y[RESTORATION_WIN2];
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * RESTORATION_WIN2);
  memset(H, 0, sizeof(*H) * RESTORATION_WIN2 * RESTORATION_WIN2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -RESTORATION_HALFWIN; k <= RESTORATION_HALFWIN; k++) {
        for (l = -RESTORATION_HALFWIN; l <= RESTORATION_HALFWIN; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      for (k = 0; k < RESTORATION_WIN2; ++k) {
        M[k] += Y[k] * X;
        H[k * RESTORATION_WIN2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < RESTORATION_WIN2; ++l) {
          double value = Y[k] * Y[l];
          H[k * RESTORATION_WIN2 + l] += value;
          H[l * RESTORATION_WIN2 + k] += value;
        }
      }
    }
  }
}

#if CONFIG_AOM_HIGHBITDEPTH
static double find_average_highbd(uint16_t *src, int h_start, int h_end,
                                  int v_start, int v_end, int stride) {
  uint64_t sum = 0;
  double avg = 0;
  int i, j;
  for (i = v_start; i < v_end; i++)
    for (j = h_start; j < h_end; j++) sum += src[i * stride + j];
  avg = (double)sum / ((v_end - v_start) * (h_end - h_start));
  return avg;
}

static void compute_stats_highbd(uint8_t *dgd8, uint8_t *src8, int h_start,
                                 int h_end, int v_start, int v_end,
                                 int dgd_stride, int src_stride, double *M,
                                 double *H) {
  int i, j, k, l;
  double Y[RESTORATION_WIN2];
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const double avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * RESTORATION_WIN2);
  memset(H, 0, sizeof(*H) * RESTORATION_WIN2 * RESTORATION_WIN2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -RESTORATION_HALFWIN; k <= RESTORATION_HALFWIN; k++) {
        for (l = -RESTORATION_HALFWIN; l <= RESTORATION_HALFWIN; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      for (k = 0; k < RESTORATION_WIN2; ++k) {
        M[k] += Y[k] * X;
        H[k * RESTORATION_WIN2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < RESTORATION_WIN2; ++l) {
          double value = Y[k] * Y[l];
          H[k * RESTORATION_WIN2 + l] += value;
          H[l * RESTORATION_WIN2 + k] += value;
        }
      }
    }
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

// Solves Ax = b, where x and b are column vectors
static int linsolve(int n, double *A, int stride, double *b, double *x) {
  int i, j, k;
  double c;
  // Partial pivoting
  for (i = n - 1; i > 0; i--) {
    if (A[(i - 1) * stride] < A[i * stride]) {
      for (j = 0; j < n; j++) {
        c = A[i * stride + j];
        A[i * stride + j] = A[(i - 1) * stride + j];
        A[(i - 1) * stride + j] = c;
      }
      c = b[i];
      b[i] = b[i - 1];
      b[i - 1] = c;
    }
  }
  // Forward elimination
  for (k = 0; k < n - 1; k++) {
    for (i = k; i < n - 1; i++) {
      c = A[(i + 1) * stride + k] / A[k * stride + k];
      for (j = 0; j < n; j++) A[(i + 1) * stride + j] -= c * A[k * stride + j];
      b[i + 1] -= c * b[k];
    }
  }
  // Backward substitution
  for (i = n - 1; i >= 0; i--) {
    if (fabs(A[i * stride + i]) < 1e-10) return 0;
    c = 0;
    for (j = i + 1; j <= n - 1; j++) c += A[i * stride + j] * x[j];
    x[i] = (b[i] - c) / A[i * stride + i];
  }
  return 1;
}

static INLINE int wrap_index(int i) {
  return (i >= RESTORATION_HALFWIN1 ? RESTORATION_WIN - 1 - i : i);
}

// Fix vector b, update vector a
static void update_a_sep_sym(double **Mc, double **Hc, double *a, double *b) {
  int i, j;
  double S[RESTORATION_WIN];
  double A[RESTORATION_WIN], B[RESTORATION_WIN2];
  int w, w2;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < RESTORATION_WIN; i++) {
    for (j = 0; j < RESTORATION_WIN; ++j) {
      const int jj = wrap_index(j);
      A[jj] += Mc[i][j] * b[i];
    }
  }
  for (i = 0; i < RESTORATION_WIN; i++) {
    for (j = 0; j < RESTORATION_WIN; j++) {
      int k, l;
      for (k = 0; k < RESTORATION_WIN; ++k)
        for (l = 0; l < RESTORATION_WIN; ++l) {
          const int kk = wrap_index(k);
          const int ll = wrap_index(l);
          B[ll * RESTORATION_HALFWIN1 + kk] +=
              Hc[j * RESTORATION_WIN + i][k * RESTORATION_WIN2 + l] * b[i] *
              b[j];
        }
    }
  }
  // Normalization enforcement in the system of equations itself
  w = RESTORATION_WIN;
  w2 = (w >> 1) + 1;
  for (i = 0; i < w2 - 1; ++i)
    A[i] -=
        A[w2 - 1] * 2 + B[i * w2 + w2 - 1] - 2 * B[(w2 - 1) * w2 + (w2 - 1)];
  for (i = 0; i < w2 - 1; ++i)
    for (j = 0; j < w2 - 1; ++j)
      B[i * w2 + j] -= 2 * (B[i * w2 + (w2 - 1)] + B[(w2 - 1) * w2 + j] -
                            2 * B[(w2 - 1) * w2 + (w2 - 1)]);
  if (linsolve(w2 - 1, B, w2, A, S)) {
    S[w2 - 1] = 1.0;
    for (i = w2; i < w; ++i) {
      S[i] = S[w - 1 - i];
      S[w2 - 1] -= 2 * S[i];
    }
    memcpy(a, S, w * sizeof(*a));
  }
}

// Fix vector a, update vector b
static void update_b_sep_sym(double **Mc, double **Hc, double *a, double *b) {
  int i, j;
  double S[RESTORATION_WIN];
  double A[RESTORATION_WIN], B[RESTORATION_WIN2];
  int w, w2;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < RESTORATION_WIN; i++) {
    const int ii = wrap_index(i);
    for (j = 0; j < RESTORATION_WIN; j++) A[ii] += Mc[i][j] * a[j];
  }

  for (i = 0; i < RESTORATION_WIN; i++) {
    for (j = 0; j < RESTORATION_WIN; j++) {
      const int ii = wrap_index(i);
      const int jj = wrap_index(j);
      int k, l;
      for (k = 0; k < RESTORATION_WIN; ++k)
        for (l = 0; l < RESTORATION_WIN; ++l)
          B[jj * RESTORATION_HALFWIN1 + ii] +=
              Hc[i * RESTORATION_WIN + j][k * RESTORATION_WIN2 + l] * a[k] *
              a[l];
    }
  }
  // Normalization enforcement in the system of equations itself
  w = RESTORATION_WIN;
  w2 = RESTORATION_HALFWIN1;
  for (i = 0; i < w2 - 1; ++i)
    A[i] -=
        A[w2 - 1] * 2 + B[i * w2 + w2 - 1] - 2 * B[(w2 - 1) * w2 + (w2 - 1)];
  for (i = 0; i < w2 - 1; ++i)
    for (j = 0; j < w2 - 1; ++j)
      B[i * w2 + j] -= 2 * (B[i * w2 + (w2 - 1)] + B[(w2 - 1) * w2 + j] -
                            2 * B[(w2 - 1) * w2 + (w2 - 1)]);
  if (linsolve(w2 - 1, B, w2, A, S)) {
    S[w2 - 1] = 1.0;
    for (i = w2; i < w; ++i) {
      S[i] = S[w - 1 - i];
      S[w2 - 1] -= 2 * S[i];
    }
    memcpy(b, S, w * sizeof(*b));
  }
}

static int wiener_decompose_sep_sym(double *M, double *H, double *a,
                                    double *b) {
  static const double init_filt[RESTORATION_WIN] = {
    0.035623, -0.127154, 0.211436, 0.760190, 0.211436, -0.127154, 0.035623,
  };
  int i, j, iter;
  double *Hc[RESTORATION_WIN2];
  double *Mc[RESTORATION_WIN];
  for (i = 0; i < RESTORATION_WIN; i++) {
    Mc[i] = M + i * RESTORATION_WIN;
    for (j = 0; j < RESTORATION_WIN; j++) {
      Hc[i * RESTORATION_WIN + j] =
          H + i * RESTORATION_WIN * RESTORATION_WIN2 + j * RESTORATION_WIN;
    }
  }
  memcpy(a, init_filt, sizeof(*a) * RESTORATION_WIN);
  memcpy(b, init_filt, sizeof(*b) * RESTORATION_WIN);

  iter = 1;
  while (iter < 10) {
    update_a_sep_sym(Mc, Hc, a, b);
    update_b_sep_sym(Mc, Hc, a, b);
    iter++;
  }
  return 1;
}

// Computes the function x'*A*x - x'*b for the learned filters, and compares
// against identity filters; Final score is defined as the difference between
// the function values
static double compute_score(double *M, double *H, int *vfilt, int *hfilt) {
  double ab[RESTORATION_WIN * RESTORATION_WIN];
  int i, k, l;
  double P = 0, Q = 0;
  double iP = 0, iQ = 0;
  double Score, iScore;
  int w;
  double a[RESTORATION_WIN], b[RESTORATION_WIN];
  w = RESTORATION_WIN;
  a[RESTORATION_HALFWIN] = b[RESTORATION_HALFWIN] = 1.0;
  for (i = 0; i < RESTORATION_HALFWIN; ++i) {
    a[i] = a[RESTORATION_WIN - i - 1] =
        (double)vfilt[i] / RESTORATION_FILT_STEP;
    b[i] = b[RESTORATION_WIN - i - 1] =
        (double)hfilt[i] / RESTORATION_FILT_STEP;
    a[RESTORATION_HALFWIN] -= 2 * a[i];
    b[RESTORATION_HALFWIN] -= 2 * b[i];
  }
  for (k = 0; k < w; ++k) {
    for (l = 0; l < w; ++l) ab[k * w + l] = a[l] * b[k];
  }
  for (k = 0; k < w * w; ++k) {
    P += ab[k] * M[k];
    for (l = 0; l < w * w; ++l) Q += ab[k] * H[k * w * w + l] * ab[l];
  }
  Score = Q - 2 * P;

  iP = M[(w * w) >> 1];
  iQ = H[((w * w) >> 1) * w * w + ((w * w) >> 1)];
  iScore = iQ - 2 * iP;

  return Score - iScore;
}

static void quantize_sym_filter(double *f, int *fi) {
  int i;
  for (i = 0; i < RESTORATION_HALFWIN; ++i) {
    fi[i] = RINT(f[i] * RESTORATION_FILT_STEP);
  }
  // Specialize for 7-tap filter
  fi[0] = CLIP(fi[0], WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP0_MAXV);
  fi[1] = CLIP(fi[1], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
  fi[2] = CLIP(fi[2], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
}

static double search_wiener(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                            int filter_level, int partial_frame,
                            RestorationInfo *info, double *best_tile_cost) {
  WienerInfo *wiener_info = info->wiener_info;
  AV1_COMMON *const cm = &cpi->common;
  RestorationInfo rsi;
  int64_t err;
  int bits;
  double cost_wiener, cost_norestore;
  MACROBLOCK *x = &cpi->td.mb;
  double M[RESTORATION_WIN2];
  double H[RESTORATION_WIN2 * RESTORATION_WIN2];
  double vfilterd[RESTORATION_WIN], hfilterd[RESTORATION_WIN];
  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  const int width = cm->width;
  const int height = cm->height;
  const int src_stride = src->y_stride;
  const int dgd_stride = dgd->y_stride;
  double score;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  int i;

  const int ntiles = av1_get_rest_ntiles(width, height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);
  assert(width == dgd->y_crop_width);
  assert(height == dgd->y_crop_height);
  assert(width == src->y_crop_width);
  assert(height == src->y_crop_height);

  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  rsi.frame_restoration_type = RESTORE_WIENER;
  rsi.wiener_info = (WienerInfo *)aom_malloc(sizeof(*rsi.wiener_info) * ntiles);
  assert(rsi.wiener_info != NULL);

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx)
    rsi.wiener_info[tile_idx].level = 0;

  // Compute best Wiener filters for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 0, 0, &h_start, &h_end,
                             &v_start, &v_end);
    err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                               v_end - v_start);
    // #bits when a tile is not restored
    bits = av1_cost_bit(RESTORE_NONE_WIENER_PROB, 0);
    cost_norestore = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    best_tile_cost[tile_idx] = DBL_MAX;

    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 1, 1, &h_start, &h_end,
                             &v_start, &v_end);
#if CONFIG_AOM_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      compute_stats_highbd(dgd->y_buffer, src->y_buffer, h_start, h_end,
                           v_start, v_end, dgd_stride, src_stride, M, H);
    else
#endif  // CONFIG_AOM_HIGHBITDEPTH
      compute_stats(dgd->y_buffer, src->y_buffer, h_start, h_end, v_start,
                    v_end, dgd_stride, src_stride, M, H);

    wiener_info[tile_idx].level = 1;
    if (!wiener_decompose_sep_sym(M, H, vfilterd, hfilterd)) {
      wiener_info[tile_idx].level = 0;
      continue;
    }
    quantize_sym_filter(vfilterd, rsi.wiener_info[tile_idx].vfilter);
    quantize_sym_filter(hfilterd, rsi.wiener_info[tile_idx].hfilter);

    // Filter score computes the value of the function x'*A*x - x'*b for the
    // learned filter and compares it against identity filer. If there is no
    // reduction in the function, the filter is reverted back to identity
    score = compute_score(M, H, rsi.wiener_info[tile_idx].vfilter,
                          rsi.wiener_info[tile_idx].hfilter);
    if (score > 0.0) {
      wiener_info[tile_idx].level = 0;
      continue;
    }

    rsi.wiener_info[tile_idx].level = 1;
    err = try_restoration_tile(src, cpi, &rsi, partial_frame, tile_idx, 0, 0);
    bits = WIENER_FILT_BITS << AV1_PROB_COST_SHIFT;
    bits += av1_cost_bit(RESTORE_NONE_WIENER_PROB, 1);
    cost_wiener = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
    if (cost_wiener >= cost_norestore) {
      wiener_info[tile_idx].level = 0;
    } else {
      wiener_info[tile_idx].level = 1;
      for (i = 0; i < RESTORATION_HALFWIN; ++i) {
        wiener_info[tile_idx].vfilter[i] = rsi.wiener_info[tile_idx].vfilter[i];
        wiener_info[tile_idx].hfilter[i] = rsi.wiener_info[tile_idx].hfilter[i];
      }
      bits = WIENER_FILT_BITS << AV1_PROB_COST_SHIFT;
      best_tile_cost[tile_idx] = RDCOST_DBL(
          x->rdmult, x->rddiv,
          (bits + cpi->switchable_restore_cost[RESTORE_WIENER]) >> 4, err);
    }
    rsi.wiener_info[tile_idx].level = 0;
  }
  // Cost for Wiener filtering
  bits = frame_level_restore_bits[rsi.frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    bits += av1_cost_bit(RESTORE_NONE_WIENER_PROB, wiener_info[tile_idx].level);
    rsi.wiener_info[tile_idx].level = wiener_info[tile_idx].level;
    if (wiener_info[tile_idx].level) {
      bits += (WIENER_FILT_BITS << AV1_PROB_COST_SHIFT);
      for (i = 0; i < RESTORATION_HALFWIN; ++i) {
        rsi.wiener_info[tile_idx].vfilter[i] = wiener_info[tile_idx].vfilter[i];
        rsi.wiener_info[tile_idx].hfilter[i] = wiener_info[tile_idx].hfilter[i];
      }
    }
  }
  err = try_restoration_frame(src, cpi, &rsi, partial_frame);
  cost_wiener = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);

  aom_free(rsi.wiener_info);

  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_wiener;
}

static double search_norestore(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                               int filter_level, int partial_frame,
                               RestorationInfo *info, double *best_tile_cost) {
  double err, cost_norestore;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  const int ntiles = av1_get_rest_ntiles(cm->width, cm->height, &tile_width,
                                         &tile_height, &nhtiles, &nvtiles);
  (void)info;

  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, cm->width, cm->height, 0, 0, &h_start,
                             &h_end, &v_start, &v_end);
    err = sse_restoration_tile(src, cm, h_start, h_end - h_start, v_start,
                               v_end - v_start);
    best_tile_cost[tile_idx] =
        RDCOST_DBL(x->rdmult, x->rddiv,
                   (cpi->switchable_restore_cost[RESTORE_NONE] >> 4), err);
  }
  // RD cost associated with no restoration
  err = sse_restoration_tile(src, cm, 0, cm->width, 0, cm->height);
  bits = frame_level_restore_bits[RESTORE_NONE] << AV1_PROB_COST_SHIFT;
  cost_norestore = RDCOST_DBL(x->rdmult, x->rddiv, (bits >> 4), err);
  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_norestore;
}

static double search_switchable_restoration(
    AV1_COMP *cpi, int filter_level, int partial_frame, RestorationInfo *rsi,
    double *tile_cost[RESTORE_SWITCHABLE_TYPES]) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *x = &cpi->td.mb;
  double cost_switchable = 0;
  int r, bits, tile_idx;
  const int ntiles =
      av1_get_rest_ntiles(cm->width, cm->height, NULL, NULL, NULL, NULL);

  //  Make a copy of the unfiltered / processed recon buffer
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_uf);
  av1_loop_filter_frame(cm->frame_to_show, cm, &cpi->td.mb.e_mbd, filter_level,
                        1, partial_frame);
  aom_yv12_copy_y(cm->frame_to_show, &cpi->last_frame_db);

  rsi->frame_restoration_type = RESTORE_SWITCHABLE;
  bits = frame_level_restore_bits[rsi->frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  cost_switchable = RDCOST_DBL(x->rdmult, x->rddiv, bits >> 4, 0);
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    double best_cost = tile_cost[RESTORE_NONE][tile_idx];
    rsi->restoration_type[tile_idx] = RESTORE_NONE;
    for (r = 1; r < RESTORE_SWITCHABLE_TYPES; r++) {
      if (tile_cost[r][tile_idx] < best_cost) {
        rsi->restoration_type[tile_idx] = r;
        best_cost = tile_cost[r][tile_idx];
      }
    }
    cost_switchable += best_cost;
  }
  aom_yv12_copy_y(&cpi->last_frame_uf, cm->frame_to_show);
  return cost_switchable;
}

void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                                 LPF_PICK_METHOD method) {
  static search_restore_type search_restore_fun[RESTORE_SWITCHABLE_TYPES] = {
    search_norestore, search_sgrproj,      search_bilateral,
    search_wiener,    search_domaintxfmrf,
  };
  AV1_COMMON *const cm = &cpi->common;
  struct loopfilter *const lf = &cm->lf;
  double cost_restore[RESTORE_TYPES];
  double *tile_cost[RESTORE_SWITCHABLE_TYPES];
  double best_cost_restore;
  RestorationType r, best_restore;

  const int ntiles =
      av1_get_rest_ntiles(cm->width, cm->height, NULL, NULL, NULL, NULL);
  cm->rst_info.restoration_type = (RestorationType *)aom_realloc(
      cm->rst_info.restoration_type,
      sizeof(*cm->rst_info.restoration_type) * ntiles);
  cm->rst_info.bilateral_info = (BilateralInfo *)aom_realloc(
      cm->rst_info.bilateral_info,
      sizeof(*cm->rst_info.bilateral_info) * ntiles);
  assert(cm->rst_info.bilateral_info != NULL);
  cm->rst_info.wiener_info = (WienerInfo *)aom_realloc(
      cm->rst_info.wiener_info, sizeof(*cm->rst_info.wiener_info) * ntiles);
  assert(cm->rst_info.wiener_info != NULL);
  cm->rst_info.sgrproj_info = (SgrprojInfo *)aom_realloc(
      cm->rst_info.sgrproj_info, sizeof(*cm->rst_info.sgrproj_info) * ntiles);
  assert(cm->rst_info.sgrproj_info != NULL);
  cm->rst_info.domaintxfmrf_info = (DomaintxfmrfInfo *)aom_realloc(
      cm->rst_info.domaintxfmrf_info,
      sizeof(*cm->rst_info.domaintxfmrf_info) * ntiles);
  assert(cm->rst_info.domaintxfmrf_info != NULL);

  for (r = 0; r < RESTORE_SWITCHABLE_TYPES; r++)
    tile_cost[r] = (double *)aom_malloc(sizeof(*tile_cost[0]) * ntiles);

  lf->sharpness_level = cm->frame_type == KEY_FRAME ? 0 : cpi->oxcf.sharpness;

  if (method == LPF_PICK_MINIMAL_LPF && lf->filter_level) {
    lf->filter_level = 0;
    cm->rst_info.frame_restoration_type = RESTORE_NONE;
  } else if (method >= LPF_PICK_FROM_Q) {
    const int min_filter_level = 0;
    const int max_filter_level = av1_get_max_filter_level(cpi);
    const int q = av1_ac_quant(cm->base_qindex, 0, cm->bit_depth);
// These values were determined by linear fitting the result of the
// searched level, filt_guess = q * 0.316206 + 3.87252
#if CONFIG_AOM_HIGHBITDEPTH
    int filt_guess;
    switch (cm->bit_depth) {
      case AOM_BITS_8:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
        break;
      case AOM_BITS_10:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 4060632, 20);
        break;
      case AOM_BITS_12:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 16242526, 22);
        break;
      default:
        assert(0 &&
               "bit_depth should be AOM_BITS_8, AOM_BITS_10 "
               "or AOM_BITS_12");
        return;
    }
#else
    int filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
#endif  // CONFIG_AOM_HIGHBITDEPTH
    if (cm->frame_type == KEY_FRAME) filt_guess -= 4;
    lf->filter_level = clamp(filt_guess, min_filter_level, max_filter_level);
  } else {
    lf->filter_level =
        av1_search_filter_level(src, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                                &cost_restore[RESTORE_NONE]);
  }
  for (r = 0; r < RESTORE_SWITCHABLE_TYPES; ++r) {
    cost_restore[r] = search_restore_fun[r](src, cpi, lf->filter_level,
                                            method == LPF_PICK_FROM_SUBIMAGE,
                                            &cm->rst_info, tile_cost[r]);
  }
  cost_restore[RESTORE_SWITCHABLE] = search_switchable_restoration(
      cpi, lf->filter_level, method == LPF_PICK_FROM_SUBIMAGE, &cm->rst_info,
      tile_cost);

  best_cost_restore = DBL_MAX;
  best_restore = 0;
  for (r = 0; r < RESTORE_TYPES; ++r) {
    if (cost_restore[r] < best_cost_restore) {
      best_restore = r;
      best_cost_restore = cost_restore[r];
    }
  }
  cm->rst_info.frame_restoration_type = best_restore;
  /*
  printf("Frame %d/%d frame_restore_type %d : %f %f %f %f %f %f\n",
         cm->current_video_frame, cm->show_frame,
         cm->rst_info.frame_restoration_type, cost_restore[0], cost_restore[1],
         cost_restore[2], cost_restore[3], cost_restore[4], cost_restore[5]);
         */
  for (r = 0; r < RESTORE_SWITCHABLE_TYPES; r++) aom_free(tile_cost[r]);
}
