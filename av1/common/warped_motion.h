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

#ifndef AV1_COMMON_WARPED_MOTION_H_
#define AV1_COMMON_WARPED_MOTION_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "./aom_config.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/mv.h"

#define MAX_PARAMDIM 9
#if CONFIG_WARPED_MOTION
#define DEFAULT_WMTYPE AFFINE
#endif  // CONFIG_WARPED_MOTION

typedef void (*ProjectPointsFunc)(int32_t *mat, int *points, int *proj,
                                  const int n, const int stride_points,
                                  const int stride_proj,
                                  const int subsampling_x,
                                  const int subsampling_y);

void project_points_translation(int32_t *mat, int *points, int *proj,
                                const int n, const int stride_points,
                                const int stride_proj, const int subsampling_x,
                                const int subsampling_y);

void project_points_rotzoom(int32_t *mat, int *points, int *proj, const int n,
                            const int stride_points, const int stride_proj,
                            const int subsampling_x, const int subsampling_y);

void project_points_affine(int32_t *mat, int *points, int *proj, const int n,
                           const int stride_points, const int stride_proj,
                           const int subsampling_x, const int subsampling_y);

void project_points_homography(int32_t *mat, int *points, int *proj,
                               const int n, const int stride_points,
                               const int stride_proj, const int subsampling_x,
                               const int subsampling_y);

void project_points(WarpedMotionParams *wm_params, int *points, int *proj,
                    const int n, const int stride_points, const int stride_proj,
                    const int subsampling_x, const int subsampling_y);

double av1_warp_erroradv(WarpedMotionParams *wm,
#if CONFIG_AOM_HIGHBITDEPTH
                         int use_hbd, int bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                         uint8_t *ref, int width, int height, int stride,
                         uint8_t *dst, int p_col, int p_row, int p_width,
                         int p_height, int p_stride, int subsampling_x,
                         int subsampling_y, int x_scale, int y_scale);

void av1_warp_plane(WarpedMotionParams *wm,
#if CONFIG_AOM_HIGHBITDEPTH
                    int use_hbd, int bd,
#endif  // CONFIG_AOM_HIGHBITDEPTH
                    uint8_t *ref, int width, int height, int stride,
                    uint8_t *pred, int p_col, int p_row, int p_width,
                    int p_height, int p_stride, int subsampling_x,
                    int subsampling_y, int x_scale, int y_scale, int ref_frm);

// Integerize model into the WarpedMotionParams structure
void av1_integerize_model(const double *model, TransformationType wmtype,
                          WarpedMotionParams *wm);

int find_translation(const int np, double *pts1, double *pts2, double *mat);
int find_rotzoom(const int np, double *pts1, double *pts2, double *mat);
int find_affine(const int np, double *pts1, double *pts2, double *mat);
int find_homography(const int np, double *pts1, double *pts2, double *mat);
int find_projection(const int np, double *pts1, double *pts2,
                    WarpedMotionParams *wm_params);
#endif  // AV1_COMMON_WARPED_MOTION_H_
