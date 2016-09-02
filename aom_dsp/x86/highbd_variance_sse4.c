/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <smmintrin.h> /* SSE4.1 */

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/variance.h"
#include "aom_dsp/aom_filter.h"

static INLINE void variance4x4_64_sse4_1(const uint8_t *a8, int a_stride,
                                         const uint8_t *b8, int b_stride,
                                         uint64_t *sse, int64_t *sum) {
  __m128i u0, u1, u2, u3;
  __m128i s0, s1, s2, s3;
  __m128i t0, t1, x0, y0;
  __m128i a0, a1, a2, a3;
  __m128i b0, b1, b2, b3;
  __m128i k_one_epi16 = _mm_set1_epi16((int16_t)1);

  uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  uint16_t *b = CONVERT_TO_SHORTPTR(b8);

  a0 = _mm_loadl_epi64((__m128i const *)(a + 0 * a_stride));
  a1 = _mm_loadl_epi64((__m128i const *)(a + 1 * a_stride));
  a2 = _mm_loadl_epi64((__m128i const *)(a + 2 * a_stride));
  a3 = _mm_loadl_epi64((__m128i const *)(a + 3 * a_stride));

  b0 = _mm_loadl_epi64((__m128i const *)(b + 0 * b_stride));
  b1 = _mm_loadl_epi64((__m128i const *)(b + 1 * b_stride));
  b2 = _mm_loadl_epi64((__m128i const *)(b + 2 * b_stride));
  b3 = _mm_loadl_epi64((__m128i const *)(b + 3 * b_stride));

  u0 = _mm_unpacklo_epi16(a0, a1);
  u1 = _mm_unpacklo_epi16(a2, a3);
  u2 = _mm_unpacklo_epi16(b0, b1);
  u3 = _mm_unpacklo_epi16(b2, b3);

  s0 = _mm_sub_epi16(u0, u2);
  s1 = _mm_sub_epi16(u1, u3);

  t0 = _mm_madd_epi16(s0, k_one_epi16);
  t1 = _mm_madd_epi16(s1, k_one_epi16);

  s2 = _mm_hadd_epi32(t0, t1);
  s3 = _mm_hadd_epi32(s2, s2);
  y0 = _mm_hadd_epi32(s3, s3);

  t0 = _mm_madd_epi16(s0, s0);
  t1 = _mm_madd_epi16(s1, s1);

  s2 = _mm_hadd_epi32(t0, t1);
  s3 = _mm_hadd_epi32(s2, s2);
  x0 = _mm_hadd_epi32(s3, s3);

  *sse = (uint64_t)_mm_extract_epi32(x0, 0);
  *sum = (int64_t)_mm_extract_epi32(y0, 0);
}

uint32_t aom_highbd_8_variance4x4_sse4_1(const uint8_t *a, int a_stride,
                                         const uint8_t *b, int b_stride,
                                         uint32_t *sse) {
  int64_t sum;
  uint64_t local_sse;

  variance4x4_64_sse4_1(a, a_stride, b, b_stride, &local_sse, &sum);
  *sse = (uint32_t)local_sse;

  return *sse - (uint32_t)((sum * sum) >> 4);
}

uint32_t aom_highbd_10_variance4x4_sse4_1(const uint8_t *a, int a_stride,
                                          const uint8_t *b, int b_stride,
                                          uint32_t *sse) {
  int64_t sum;
  uint64_t local_sse;

  variance4x4_64_sse4_1(a, a_stride, b, b_stride, &local_sse, &sum);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(local_sse, 4);
  sum = ROUND_POWER_OF_TWO(sum, 2);

  return *sse - (uint32_t)((sum * sum) >> 4);
}

uint32_t aom_highbd_12_variance4x4_sse4_1(const uint8_t *a, int a_stride,
                                          const uint8_t *b, int b_stride,
                                          uint32_t *sse) {
  int64_t sum;
  uint64_t local_sse;

  variance4x4_64_sse4_1(a, a_stride, b, b_stride, &local_sse, &sum);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(local_sse, 8);
  sum = ROUND_POWER_OF_TWO(sum, 4);

  return *sse - (uint32_t)((sum * sum) >> 4);
}

// Sub-pixel
uint32_t aom_highbd_8_sub_pixel_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  return aom_highbd_8_variance4x4(CONVERT_TO_BYTEPTR(temp2), 4, dst, dst_stride,
                                  sse);
}

uint32_t aom_highbd_10_sub_pixel_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  return aom_highbd_10_variance4x4(CONVERT_TO_BYTEPTR(temp2), 4, dst,
                                   dst_stride, sse);
}

uint32_t aom_highbd_12_sub_pixel_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  return aom_highbd_12_variance4x4(CONVERT_TO_BYTEPTR(temp2), 4, dst,
                                   dst_stride, sse);
}

// Sub-pixel average

uint32_t aom_highbd_8_sub_pixel_avg_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse,
    const uint8_t *second_pred) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];
  DECLARE_ALIGNED(16, uint16_t, temp3[4 * 4]);

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  aom_highbd_comp_avg_pred(temp3, second_pred, 4, 4, CONVERT_TO_BYTEPTR(temp2),
                           4);

  return aom_highbd_8_variance4x4(CONVERT_TO_BYTEPTR(temp3), 4, dst, dst_stride,
                                  sse);
}

uint32_t aom_highbd_10_sub_pixel_avg_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse,
    const uint8_t *second_pred) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];
  DECLARE_ALIGNED(16, uint16_t, temp3[4 * 4]);

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  aom_highbd_comp_avg_pred(temp3, second_pred, 4, 4, CONVERT_TO_BYTEPTR(temp2),
                           4);

  return aom_highbd_10_variance4x4(CONVERT_TO_BYTEPTR(temp3), 4, dst,
                                   dst_stride, sse);
}

uint32_t aom_highbd_12_sub_pixel_avg_variance4x4_sse4_1(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, uint32_t *sse,
    const uint8_t *second_pred) {
  uint16_t fdata3[(4 + 1) * 4];
  uint16_t temp2[4 * 4];
  DECLARE_ALIGNED(16, uint16_t, temp3[4 * 4]);

  aom_highbd_var_filter_block2d_bil_first_pass(
      src, fdata3, src_stride, 1, 4 + 1, 4, bilinear_filters_2t[xoffset]);
  aom_highbd_var_filter_block2d_bil_second_pass(fdata3, temp2, 4, 4, 4, 4,
                                                bilinear_filters_2t[yoffset]);

  aom_highbd_comp_avg_pred(temp3, second_pred, 4, 4, CONVERT_TO_BYTEPTR(temp2),
                           4);

  return aom_highbd_12_variance4x4(CONVERT_TO_BYTEPTR(temp3), 4, dst,
                                   dst_stride, sse);
}