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

#include <smmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <float.h>

#include "./av1_rtcd.h"
#include "av1/common/x86/dist_sse4.h"
#include "av1/common/pvq.h"

#define OD_DIST_LP_MID (5)
#define OD_DIST_LP_NORM (OD_DIST_LP_MID + 2)

static __m128 horizontal_sum_ps(__m128 x) {
  x = _mm_add_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(1, 0, 3, 2)));
  x = _mm_add_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1)));
  return x;
}

/* Approximation of sqrt: x / rsqrt(x) */
static __m128 sqrt_ps(__m128 x) {
  return _mm_mul_ps(x, _mm_rsqrt_ps(_mm_add_ps(_mm_set_ps1(FLT_MIN), x)));
}

static __m128i horizontal_sum_epi32(__m128i x) {
  x = _mm_add_epi32(x, _mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 3, 2)));
  x = _mm_add_epi32(x, _mm_shuffle_epi32(x, _MM_SHUFFLE(2, 3, 0, 1)));
  return x;
}

static __m128i horizontal_sum_epi64(__m128i x) {
  return _mm_add_epi64(x, _mm_srli_si128(x, 8));
}

void print_ints(const char *name, int *v, int n) {
  printf("%16s = (", name);
  for (int i = 0; i < n; i++) {
    if (i) {
      printf(",");
    }
    printf("%3d", v[i]);
  }
  printf(") [%d]\n", n);
}

int od_compute_var_4x4_sse4_1(int *x, int stride) {
  int i;
  __m128i sum = _mm_setzero_si128();
  __m128i sum2 = _mm_setzero_si128();
  for (i = 0; i < 4; i++) {
    __m128i x4 = _mm_loadu_si128((__m128i *)&x[i * stride]);
    sum = _mm_add_epi32(sum, x4);
    sum2 = _mm_add_epi32(sum2, _mm_mullo_epi32(x4, x4));
  }
  int isum = _mm_cvtsi128_si32(horizontal_sum_epi32(sum));
  int isum2 = _mm_cvtsi128_si32(horizontal_sum_epi32(sum2));
  // TODO(yushin) : Check wheter any changes are required for high bit depth.
  return (isum2 - (isum * isum >> 4)) >> 4;
}

double od_compute_dist_8x8_sse4_1(int qm, int use_activity_masking, int *x,
                                  int *y, int *e_lp, int stride) {
  double sum;
  double var_stat;
  double activity;
  double calibration;
  int i;
  int j;
  OD_ASSERT(qm != OD_FLAT_QM);
#if 1
  int varx[12];
  int vary[12];
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      varx[i * 3 + j] =
          od_compute_var_4x4_sse4_1(x + 2 * i * stride + 2 * j, stride);
      vary[i * 3 + j] =
          od_compute_var_4x4_sse4_1(y + 2 * i * stride + 2 * j, stride);
    }
  }
  varx[9] = varx[10] = varx[11] = 0;
  vary[9] = vary[10] = vary[11] = 0;
  __m128 varx4a = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&varx[0]));
  __m128 varx4b = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&varx[4]));
  __m128 varx4c = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&varx[8]));

  /* We use a different variance statistic depending on whether activity
     masking is used, since the harmonic mean appeared slghtly worse with
     masking off. The calibration constant just ensures that we preserve the
     rate compared to activity=1. */
  if (use_activity_masking) {
    /* Compute: for (i = 0; i < 9; i++) mean_var += 1. / (1 + varx[i]); */
    __m128 one = _mm_set_ps1(1.f);
    __m128 mean_var4 = _mm_div_ps(one, _mm_add_ps(one, varx4a));
    mean_var4 = _mm_add_ps(mean_var4, _mm_div_ps(one, _mm_add_ps(one, varx4b)));
    mean_var4 = _mm_add_ps(mean_var4, _mm_div_ps(one, _mm_add_ps(one, varx4c)));
    float mean_var = _mm_cvtss_f32(horizontal_sum_ps(mean_var4)) - 3;

    calibration = 1.95;
    var_stat = 9. / mean_var;
  } else {
    int min_var = INT_MAX;
    for (i = 0; i < 9; i++) {
      min_var = OD_MINI(min_var, varx[i]);
    }
    calibration = 1.62;
    var_stat = min_var;
  }
  /* 1.62 is a calibration constant, 0.25 is a noise floor and 1/6 is the
     activity masking constant. */
  activity = calibration * pow(.25 + var_stat, -1. / 6);
#else
  activity = 1;
#endif
  /* Accumulate sum of squares in a 64-bit integer. This is different from the
   * C code which uses a double to do that. */
  __m128i sum2 = _mm_setzero_si128();
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j += 4) {
      __m128i e4 = _mm_loadu_si128((__m128i *)&e_lp[i * stride + j]);
      __m128i lo = _mm_unpacklo_epi32(e4, _mm_setzero_si128());
      __m128i hi = _mm_unpackhi_epi32(e4, _mm_setzero_si128());
      sum2 = _mm_add_epi64(sum2, _mm_mullo_epi32(lo, lo));
      sum2 = _mm_add_epi64(sum2, _mm_mullo_epi32(hi, hi));
    }
  }
  sum = (double)_mm_cvtsi128_si64(horizontal_sum_epi64(sum2));

  /* Normalize the filter to unit DC response. */
  sum *= 1. / (OD_DIST_LP_NORM * OD_DIST_LP_NORM * OD_DIST_LP_NORM *
               OD_DIST_LP_NORM);

  /* Compute: for (i = 0; i < 9; i++) vardist += varx[i] - 2 * sqrt(varx[i] *
   * vary[i]) + vary[i]; */
  __m128 vary4a = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&vary[0]));
  __m128 vary4b = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&vary[4]));
  __m128 vary4c = _mm_cvtepi32_ps(_mm_loadu_si128((__m128i *)&vary[8]));
  __m128 varxy4a = _mm_mul_ps(varx4a, vary4a);
  __m128 varxy4b = _mm_mul_ps(varx4b, vary4b);
  __m128 varxy4c = _mm_mul_ps(varx4c, vary4c);
  __m128 two = _mm_set_ps1(2.f);
  __m128 vardist4 = _mm_add_ps(_mm_add_ps(varx4a, _mm_add_ps(varx4b, varx4c)),
                               _mm_add_ps(vary4a, _mm_add_ps(vary4b, vary4c)));
  vardist4 = _mm_sub_ps(vardist4, _mm_mul_ps(two, sqrt_ps(varxy4a)));
  vardist4 = _mm_sub_ps(vardist4, _mm_mul_ps(two, sqrt_ps(varxy4b)));
  vardist4 = _mm_sub_ps(vardist4, _mm_mul_ps(two, sqrt_ps(varxy4c)));
  double vardist = _mm_cvtss_f32(horizontal_sum_ps(vardist4));

  double result = activity * activity * (sum + vardist);
  // double c_result = od_compute_dist_8x8_c(qm, use_activity_masking, x, y,
  // e_lp, stride);
  // if (result != c_result) {
  //   printf("%lf %lf\n", result, c_result);
  // }
  return result;
}