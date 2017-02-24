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
#include <alloca.h>

#include "./av1_rtcd.h"
#include "av1/common/x86/pvq_sse4.h"
#include "../odintrin.h"

#define VARDECL(type, var) type *var
#define ALLOC(var, size, type) var = ((type*)alloca(sizeof(type)*(size)))
#define SAVE_STACK
#define RESTORE_STACK
#define ALLOC_STACK
#define ALLOC_NONE 0
#define EPSILON 1e-15f

#define ADD16(a,b) ((a)+(b))
#define SUB16(a,b) ((a)-(b))
#define ADD32(a,b) ((a)+(b))
#define SUB32(a,b) ((a)-(b))
#define EXTEND32(x) (x)
#define MAC16_16(c,a,b) ((c)+(od_val32)(a)*(od_val32)(b))
#define QCONST16(x,bits) (x)
#define QCONST32(x,bits) (x)

#define PRINT 1
static int xxx = 0;
void print_floats(const char *name, float *v, int n) {
  if (xxx ++ > 1000) return;
  if (!PRINT) return;
  printf("%16s = (", name);
  for (int i = 0; i < n; i++) {
    if (i) {
      printf(",");
    }
    printf("%8.2f", v[i]);
  }
  printf(") [%d]\n", n);
}

void print_ints(const char *name, int *v, int n) {
  if (xxx ++ > 1000) return;
  if (!PRINT) return;
  printf("%16s = (", name);
  for (int i = 0; i < n; i++) {
    if (i) {
      printf(",");
    }
    printf("%3d", v[i]);
  }
  printf(") [%d]\n", n);
}

void print__m128_ps(const char *name, __m128 v) {
  if (xxx ++ > 1000) return;
  if (!PRINT) return;
  print_floats(name, (float *)&v, 4);
}

static __m128 horizontal_sum_ps(__m128 x) {
  x = _mm_add_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(1, 0, 3, 2)));
  x = _mm_add_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1)));
  return x;
}
static __m128i horizontal_sum_epi32(__m128i x) {
  x = _mm_add_epi32(x, _mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 3, 2)));
  x = _mm_add_epi32(x, _mm_shuffle_epi32(x, _MM_SHUFFLE(2, 3, 0, 1)));
  return x;
}

// s: single-precision floating point
// d: double-precision floating point
// i128: signed 128-bit integer
// i64: signed 64-bit integer
// u64: unsigned 64-bit integer
// i32: signed 32-bit integer
// u32: unsigned 32-bit integer
// i16: signed 16-bit integer
// u16: unsigned 16-bit integer
// i8: signed 8-bit integer
// u8: unsigned 8-bit integer
// ps: packed single-precision floating point
// pd: packed double-precision floating point
// sd: scalar double-precision floating point
// epi32: extended packed 32-bit integer
// si256: scalar 256-bit integer

int getenv_int(const char* name) {
  char *env = getenv(name);
  if (env && *env) {
    return (int)strtol(env, NULL, 0);
  }
  return 0;
}

// static xxx = 0;
// static yyy = 0;
/** Find the codepoint on the given PSphere closest to the desired
 * vector. float-precision PVQ search just to make sure our tests
 * aren't limited by numerical accuracy.
 *
 * @param [in]      xcoeff  input vector to quantize (x in the math doc)
 * @param [in]      n       number of dimensions
 * @param [in]      k       number of pulses
 * @param [in]      g2      multiplier for the distortion (typically squared
 *                          gain units)
 * @param [in] pvq_norm_lambda enc->pvq_norm_lambda for quantized RDO
 * @param [in]      prev_k  number of pulses already in ypulse that we should
 *                          reuse for the search (or 0 for a new search)
 * @return                  cosine distance between x and y (between 0 and 1)
 */
float pvq_search_rdo_float_sse4_1(const int16_t *xcoeff, int n, int k,
    int *ypulse, float g2, float pvq_norm_lambda, int prev_k) {
  int i, j;
  int pulses_left;
  int reuse_pulses = prev_k > 0 && prev_k <= k;
  // return pvq_search_rdo_float_c(xcoeff, n, k, ypulse, g2, pvq_norm_lambda, prev_k);
  /* TODO - This blows our 8kB stack space budget and should be fixed when
   converting PVQ to fixed point. */
  float xx = 0, xy = 0, yy = 0;
  float *y;
  float *x;
  float *sign_y;
  __m128 sums;

  (void)g2;
  (void)pvq_norm_lambda;
  (void)prev_k;

  ALLOC(y, n + 3, float);
  ALLOC(x, n + 3, float);
  ALLOC(sign_y, n + 3, float);

  for (i = 0; i < n; i++) {
    float tmp = fabsf((float)xcoeff[i]);
    xx += tmp * tmp;
    x[i] = xcoeff[i];
  }
  // https://jmvalin.ca/video/norm_simplify.patch
  float lambda = 0.5f * sqrtf(xx) * pvq_norm_lambda / (1e-30 + g2);
  float delta_rate = 3.f / n;

  x[n] = x[n + 1] = x[n + 2] = 0;
  ypulse[n] = ypulse[n + 1] = ypulse[n + 2] = 0;

  sums = _mm_setzero_ps();
  for (i = 0; i < n; i += 4) {
    __m128 x4 = _mm_loadu_ps(&x[i]);
    __m128 s4 = _mm_cmplt_ps(x4, _mm_setzero_ps());
    /* Save the sign. */
    _mm_storeu_ps(&sign_y[i], s4);
    /* Get rid of the sign. */
    x4 = _mm_andnot_ps(_mm_set_ps1(-0.f), x4);
    sums = _mm_add_ps(sums, x4);
    if (!reuse_pulses) {
      /* Clear y and ypulse in case we don't do the projection. */
      _mm_storeu_ps(&y[i], _mm_setzero_ps());
      _mm_storeu_si128((__m128i *)&ypulse[i], _mm_setzero_si128());
    }
    _mm_storeu_ps(&x[i], x4);
  }
  sums = horizontal_sum_ps(sums);

  pulses_left = k;
  {
    __m128i pulses_sum;
    __m128 yy4, xy4;
    xy4 = yy4 = _mm_setzero_ps();
    pulses_sum = _mm_setzero_si128();
    if (reuse_pulses) {
      /* We reuse pulses from a previous search so we don't have to search them
          again. */
      for (j = 0; j < n; j += 4) {
        __m128 x4, y4;
        __m128i iy4;
        iy4 = _mm_abs_epi32(_mm_loadu_si128((__m128i *)&ypulse[j]));
        pulses_sum = _mm_add_epi32(pulses_sum, iy4);
        _mm_storeu_si128((__m128i *)&ypulse[j], iy4);
        y4 = _mm_cvtepi32_ps(iy4);
        x4 = _mm_loadu_ps(&x[j]);
        xy4 = _mm_add_ps(xy4, _mm_mul_ps(x4, y4));
        yy4 = _mm_add_ps(yy4, _mm_mul_ps(y4, y4));
        /* double the y[] vector so we don't have to do it in the search loop. */
        _mm_storeu_ps(&y[j], _mm_add_ps(y4, y4));
      }
      pulses_left -= _mm_cvtsi128_si32(horizontal_sum_epi32(pulses_sum));
      xy4 = horizontal_sum_ps(xy4);
      xy = _mm_cvtss_f32(xy4);
      yy4 = horizontal_sum_ps(yy4);
      yy = _mm_cvtss_f32(yy4);
    }
    else if (k > (n >> 1)) {
      /* Do a pre-search by projecting on the pyramid */
      __m128 rcp4;
      float sum = _mm_cvtss_f32(sums);
      /* If x is too small, just replace it with a pulse at 0. This prevents infinities
          and NaNs from causing too many pulses to be allocated. Here, 64 is an approximation
          of infinity. */
      if (sum <= EPSILON) {
        x[0] = 1.f;
        for (i = 1; i < n; i++) {
          x[i] = 0;
        }
        sums = _mm_set_ps1(1.f);
      }
      /* Using k + e with e < 1 guarantees we cannot get more than k pulses. */
      rcp4 = _mm_mul_ps(_mm_set_ps1((float)(k + .8)), _mm_rcp_ps(sums));
      xy4 = yy4 = _mm_setzero_ps();
      pulses_sum = _mm_setzero_si128();
      for (j = 0; j < n; j += 4) {
        __m128 rx4, x4, y4;
        __m128i iy4;
        x4 = _mm_loadu_ps(&x[j]);
        rx4 = _mm_mul_ps(x4, rcp4);
        iy4 = _mm_cvttps_epi32(rx4);
        pulses_sum = _mm_add_epi32(pulses_sum, iy4);
        _mm_storeu_si128((__m128i *)&ypulse[j], iy4);
        y4 = _mm_cvtepi32_ps(iy4);
        xy4 = _mm_add_ps(xy4, _mm_mul_ps(x4, y4));
        yy4 = _mm_add_ps(yy4, _mm_mul_ps(y4, y4));
        /* double the y[] vector so we don't have to do it in the search loop. */
        _mm_storeu_ps(&y[j], _mm_add_ps(y4, y4));
      }
      pulses_left -= _mm_cvtsi128_si32(horizontal_sum_epi32(pulses_sum));
      xy = _mm_cvtss_f32(horizontal_sum_ps(xy4));
      yy = _mm_cvtss_f32(horizontal_sum_ps(yy4));
    }

    x[n] = x[n + 1] = x[n + 2] = -100;
    y[n] = y[n + 1] = y[n + 2] =  100;
  }
  // printf("%d - %d\n", pulses_left, n);
  // celt_assert2(pulses_left>=0, "Allocated too many pulses in the quick pass");

  /* This should never happen, but just in case it does (e.g. on silence)
      we fill the first bin with pulses. */
  OD_ASSERT(pulses_left <= n + 3);
  // if (pulses_left > n + 3) {
  //   od_val16 tmp = (od_val16)pulses_left;
  //   yy = MAC16_16(yy, tmp, tmp);
  //   yy = MAC16_16(yy, tmp, y[0]);
  //   ypulse[0] += pulses_left;
  //   pulses_left = 0;
  // }

  for (i = 0; i < pulses_left; i++) {
    int best_id = 0;
    __m128 xy4, yy4;
    __m128 max, max2;
    __m128i count;
    __m128i pos;

    /* The squared magnitude term gets added anyway, so we might as well
        add it outside the loop */
    yy = ADD16(yy, 1);
    xy4 = _mm_load1_ps(&xy);
    yy4 = _mm_load1_ps(&yy);
    max = _mm_setzero_ps();
    pos = _mm_setzero_si128();
    count = _mm_set_epi32(3, 2, 1, 0);
    for (j = 0; j < n; j += 4) {
      __m128 x4, y4, r4;
      x4 = _mm_loadu_ps(&x[j]);
      y4 = _mm_loadu_ps(&y[j]);
      x4 = _mm_add_ps(x4, xy4);
      y4 = _mm_add_ps(y4, yy4);
      y4 = _mm_rsqrt_ps(y4);
      r4 = _mm_mul_ps(x4, y4);
      /* Subtract lambda */
      r4 = _mm_sub_ps(r4, _mm_mul_ps(_mm_cvtepi32_ps(count), _mm_set_ps1(lambda * delta_rate)));

      /* Update the index of the max. */
      pos = _mm_max_epi16(pos, _mm_and_si128(count, _mm_castps_si128(_mm_cmpgt_ps(r4, max))));
      /* Update the max. */
      max = _mm_max_ps(max, r4);
      /* Update the indices (+4) */
      count = _mm_add_epi32(count, _mm_set_epi32(4, 4, 4, 4));
    }
    /* Horizontal max */
    max2 = _mm_max_ps(max, _mm_shuffle_ps(max, max, _MM_SHUFFLE(1, 0, 3, 2)));
    max2 = _mm_max_ps(max2, _mm_shuffle_ps(max2, max2, _MM_SHUFFLE(2, 3, 0, 1)));
    /* Now that max2 contains the max at all positions, look at which value(s) of the
        partial max is equal to the global max. */
    pos = _mm_and_si128(pos, _mm_castps_si128(_mm_cmpeq_ps(max, max2)));
    pos = _mm_max_epi16(pos, _mm_unpackhi_epi64(pos, pos));
    pos = _mm_max_epi16(pos, _mm_shufflelo_epi16(pos, _MM_SHUFFLE(1, 0, 3, 2)));
    best_id = _mm_cvtsi128_si32(pos);
    OD_ASSERT(best_id < n);

    /* Updating the sums of the new pulse(s) */
    xy = ADD32(xy, EXTEND32(x[best_id]));
    /* We're multiplying y[j] by two so we don't have to do it here */
    yy = ADD16(yy, y[best_id]);

    /* Only now that we've made the final choice, update y/ypulse */
    /* Multiplying y[j] by 2 so we don't have to do it everywhere else */
    y[best_id] += 2;
    ypulse[best_id]++;
  }

  /* Put the original sign back */
  for (i = 0; i < n; i += 4) {
    __m128i y4;
    __m128i s4;
    y4 = _mm_loadu_si128((__m128i *)&ypulse[i]);
    s4 = _mm_castps_si128(_mm_loadu_ps(&sign_y[i]));
    y4 = _mm_xor_si128(_mm_add_epi32(y4, s4), s4);
    _mm_storeu_si128((__m128i *)&ypulse[i], y4);
  }

  return xy / (1e-100 + sqrtf(xx * yy));
}