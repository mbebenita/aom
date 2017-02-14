/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "aom_dsp/entcode.h"
#include "aom_dsp/entenc.h"
#include "av1/common/blockd.h"
#include "av1/common/odintrin.h"
#include "av1/common/partition.h"
#include "av1/common/pvq_state.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/pvq_encoder.h"

#include <emmintrin.h>

#define OD_PVQ_RATE_APPROX (0)
/*Shift to ensure that the upper bound (i.e. for the max blocksize) of the
   dot-product of the 1st band of chroma with the luma ref doesn't overflow.*/
#define OD_CFL_FLIP_SHIFT (OD_LIMIT_BSIZE_MAX + 0)

static void aom_encode_pvq_codeword(aom_writer *w, od_pvq_codeword_ctx *adapt,
 const od_coeff *in, int n, int k) {
  int i;
  aom_encode_band_pvq_splits(w, adapt, in, n, k, 0);
  for (i = 0; i < n; i++) if (in[i]) aom_write_bit(w, in[i] < 0);
}

/* Computes 1/sqrt(i) using a table for small values. */
static float od_rsqrt_table(int i) {
  static float table[16] = {
    1.000000, 0.707107, 0.577350, 0.500000,
    0.447214, 0.408248, 0.377964, 0.353553,
    0.333333, 0.316228, 0.301511, 0.288675,
    0.277350, 0.267261, 0.258199, 0.250000};
  if (i <= 16) return table[i-1];
  else return 1./sqrt(i);
}

/*Computes 1/sqrt(start+2*i+1) using a lookup table containing the results
   where 0 <= i < table_size.*/
static float od_custom_rsqrt_dynamic_table(const float* table,
 const int table_size, const float start, const int i) {
  if (i < table_size) return table[i];
  else return od_rsqrt_table((int)(start + 2*i + 1));
}

/*Fills tables used in od_custom_rsqrt_dynamic_table for a given start.*/
static void od_fill_dynamic_rsqrt_table(float *table, const int table_size,
 const float start) {
  int i;
  for (i = 0; i < table_size; i++)
    table[i] = od_rsqrt_table((int)(start + 2*i + 1));
}

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

// static float pvq_search_rdo_float_c(const od_val16 *xcoeff, int n, int k,
//  od_coeff *ypulse, float g2, float pvq_norm_lambda, int prev_k) {

static float pvq_search_rdo_float(const od_val16 *_X, int N, int K, od_coeff *iy, float g2, float pvq_norm_lambda, int prev_k)
{
   int i, j;
   int pulsesLeft;

   /* TODO - This blows our 8kB stack space budget and should be fixed when
   converting PVQ to fixed point. */
   float x[MAXN];
   float xx, xy, yy;
   VARDECL(float, y);
   VARDECL(float, X);
   VARDECL(float, signy);
   __m128 signmask;
   __m128 sums;
   __m128i fours;
   SAVE_STACK;
   (void)g2;
   (void)pvq_norm_lambda;
   (void)prev_k;
  xx = xy = yy = 0;
  for (j = 0; j < N; j++) {
    x[j] = fabs((float)_X[j]);
    xx += x[j]*x[j];
  }

   // (void)arch;
   /* All bits set to zero, except for the sign bit. */
   signmask = _mm_set_ps1(-0.f);
   fours = _mm_set_epi32(4, 4, 4, 4);
   ALLOC(y, N+3, float);
   ALLOC(X, N+3, float);
   ALLOC(signy, N+3, float);

   for (j=0;j<N;j++) {
     X[j] = _X[j];
   }
   // OD_COPY(X, _X, N);

   X[N] = X[N+1] = X[N+2] = 0;
   sums = _mm_setzero_ps();
   for (j=0;j<N;j+=4)
   {
      __m128 x4, s4;
      x4 = _mm_loadu_ps(&X[j]);
      s4 = _mm_cmplt_ps(x4, _mm_setzero_ps());
      /* Get rid of the sign */
      x4 = _mm_andnot_ps(signmask, x4);
      sums = _mm_add_ps(sums, x4);
      /* Clear y and iy in case we don't do the projection. */
      _mm_storeu_ps(&y[j], _mm_setzero_ps());
      _mm_storeu_si128((__m128i*)&iy[j], _mm_setzero_si128());
      _mm_storeu_ps(&X[j], x4);
      _mm_storeu_ps(&signy[j], s4);
   }
   sums = _mm_add_ps(sums, _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(1, 0, 3, 2)));
   sums = _mm_add_ps(sums, _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(2, 3, 0, 1)));

   xy = yy = 0;

   pulsesLeft = K;

   /* Do a pre-search by projecting on the pyramid */
   if (K > (N>>1))
   {
      __m128i pulses_sum;
      __m128 yy4, xy4;
      __m128 rcp4;
      float sum = _mm_cvtss_f32(sums);
      /* If X is too small, just replace it with a pulse at 0 */
      /* Prevents infinities and NaNs from causing too many pulses
         to be allocated. 64 is an approximation of infinity here. */
      if (!(sum > EPSILON && sum < 64))
      {
         X[0] = QCONST16(1.f,14);
         j=1; do
            X[j]=0;
         while (++j<N);
         sums = _mm_set_ps1(1.f);
      }
      /* Using K+e with e < 1 guarantees we cannot get more than K pulses. */
      rcp4 = _mm_mul_ps(_mm_set_ps1((float)(K+.8)), _mm_rcp_ps(sums));
      xy4 = yy4 = _mm_setzero_ps();
      pulses_sum = _mm_setzero_si128();
      for (j=0;j<N;j+=4)
      {
         __m128 rx4, x4, y4;
         __m128i iy4;
         x4 = _mm_loadu_ps(&X[j]);
         rx4 = _mm_mul_ps(x4, rcp4);
         iy4 = _mm_cvttps_epi32(rx4);
         pulses_sum = _mm_add_epi32(pulses_sum, iy4);
         _mm_storeu_si128((__m128i*)&iy[j], iy4);
         y4 = _mm_cvtepi32_ps(iy4);
         xy4 = _mm_add_ps(xy4, _mm_mul_ps(x4, y4));
         yy4 = _mm_add_ps(yy4, _mm_mul_ps(y4, y4));
         /* double the y[] vector so we don't have to do it in the search loop. */
         _mm_storeu_ps(&y[j], _mm_add_ps(y4, y4));
      }
      pulses_sum = _mm_add_epi32(pulses_sum, _mm_shuffle_epi32(pulses_sum, _MM_SHUFFLE(1, 0, 3, 2)));
      pulses_sum = _mm_add_epi32(pulses_sum, _mm_shuffle_epi32(pulses_sum, _MM_SHUFFLE(2, 3, 0, 1)));
      pulsesLeft -= _mm_cvtsi128_si32(pulses_sum);
      xy4 = _mm_add_ps(xy4, _mm_shuffle_ps(xy4, xy4, _MM_SHUFFLE(1, 0, 3, 2)));
      xy4 = _mm_add_ps(xy4, _mm_shuffle_ps(xy4, xy4, _MM_SHUFFLE(2, 3, 0, 1)));
      xy = _mm_cvtss_f32(xy4);
      yy4 = _mm_add_ps(yy4, _mm_shuffle_ps(yy4, yy4, _MM_SHUFFLE(1, 0, 3, 2)));
      yy4 = _mm_add_ps(yy4, _mm_shuffle_ps(yy4, yy4, _MM_SHUFFLE(2, 3, 0, 1)));
      yy = _mm_cvtss_f32(yy4);
   }
   X[N] = X[N+1] = X[N+2] = -100;
   y[N] = y[N+1] = y[N+2] = 100;
   // celt_assert2(pulsesLeft>=0, "Allocated too many pulses in the quick pass");

   /* This should never happen, but just in case it does (e.g. on silence)
      we fill the first bin with pulses. */
   if (pulsesLeft > N+3)
   {
      od_val16 tmp = (od_val16)pulsesLeft;
      yy = MAC16_16(yy, tmp, tmp);
      yy = MAC16_16(yy, tmp, y[0]);
      iy[0] += pulsesLeft;
      pulsesLeft=0;
   }

   for (i=0;i<pulsesLeft;i++)
   {
      int best_id;
      __m128 xy4, yy4;
      __m128 max, max2;
      __m128i count;
      __m128i pos;
      best_id = 0;
      /* The squared magnitude term gets added anyway, so we might as well
         add it outside the loop */
      yy = ADD16(yy, 1);
      xy4 = _mm_load1_ps(&xy);
      yy4 = _mm_load1_ps(&yy);
      max = _mm_setzero_ps();
      pos = _mm_setzero_si128();
      count = _mm_set_epi32(3, 2, 1, 0);
      for (j=0;j<N;j+=4)
      {
         __m128 x4, y4, r4;
         x4 = _mm_loadu_ps(&X[j]);
         y4 = _mm_loadu_ps(&y[j]);
         x4 = _mm_add_ps(x4, xy4);
         y4 = _mm_add_ps(y4, yy4);
         y4 = _mm_rsqrt_ps(y4);
         r4 = _mm_mul_ps(x4, y4);
         /* Update the index of the max. */
         pos = _mm_max_epi16(pos, _mm_and_si128(count, _mm_castps_si128(_mm_cmpgt_ps(r4, max))));
         /* Update the max. */
         max = _mm_max_ps(max, r4);
         /* Update the indices (+4) */
         count = _mm_add_epi32(count, fours);
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

      /* Updating the sums of the new pulse(s) */
      xy = ADD32(xy, EXTEND32(X[best_id]));
      /* We're multiplying y[j] by two so we don't have to do it here */
      yy = ADD16(yy, y[best_id]);

      /* Only now that we've made the final choice, update y/iy */
      /* Multiplying y[j] by 2 so we don't have to do it everywhere else */
      y[best_id] += 2;
      iy[best_id]++;
   }

   /* Put the original sign back */
   for (j=0;j<N;j+=4)
   {
      __m128i y4;
      __m128i s4;
      y4 = _mm_loadu_si128((__m128i*)&iy[j]);
      s4 = _mm_castps_si128(_mm_loadu_ps(&signy[j]));
      y4 = _mm_xor_si128(_mm_add_epi32(y4, s4), s4);
      _mm_storeu_si128((__m128i*)&iy[j], y4);
   }
   RESTORE_STACK;
   return xy/(1e-100 + sqrtf(xx*yy));
}

/** Find the codepoint on the given PSphere closest to the desired
 * vector. float-precision PVQ search just to make sure our tests
 * aren't limited by numerical accuracy.
 *
 * @param [in]      xcoeff  input vector to quantize (x in the math doc)
 * @param [in]      n       number of dimensions
 * @param [in]      k       number of pulses
 * @param [out]     ypulse  optimal codevector found (y in the math doc)
 * @param [out]     g2      multiplier for the distortion (typically squared
 *                          gain units)
 * @param [in] pvq_norm_lambda enc->pvq_norm_lambda for quantized RDO
 * @param [in]      prev_k  number of pulses already in ypulse that we should
 *                          reuse for the search (or 0 for a new search)
 * @return                  cosine distance between x and y (between 0 and 1)
 */
static float pvq_search_rdo_float_old(const od_val16 *xcoeff, int n, int k,
 od_coeff *ypulse, float g2, float pvq_norm_lambda, int prev_k) {

  // printf("xcoeff = %p, ypulse = %p, n = %4d, k = %4d, g2 = %10f, pvq_norm_lambda = %10f, prev_k = %4d\n", xcoeff, ypulse, n, k, g2, pvq_norm_lambda, prev_k);

  int i, j;
  float xy;
  float yy;
  /* TODO - This blows our 8kB stack space budget and should be fixed when
   converting PVQ to fixed point. */
  float x[MAXN];
  float xx;
  float lambda;
  float norm_1;
  int rdo_pulses;
  float delta_rate;
  xx = xy = yy = 0;
  for (j = 0; j < n; j++) {
    x[j] = fabs((float)xcoeff[j]);
    xx += x[j]*x[j];
  }
  norm_1 = 1./sqrtf(1e-30 + xx);
  lambda = pvq_norm_lambda/(1e-30 + g2);
  i = 0;
  if (prev_k > 0 && prev_k <= k) {
    /* We reuse pulses from a previous search so we don't have to search them
       again. */
    for (j = 0; j < n; j++) {
      ypulse[j] = abs(ypulse[j]);
      xy += x[j]*ypulse[j];
      yy += ypulse[j]*ypulse[j];
      i += ypulse[j];
    }
  }
  else if (k > 2) {
    float l1_norm;
    float l1_inv;
    l1_norm = 0;
    for (j = 0; j < n; j++) l1_norm += x[j];
    l1_inv = 1./OD_MAXF(l1_norm, 1e-100);
    for (j = 0; j < n; j++) {
      float tmp;
      tmp = k*x[j]*l1_inv;
      ypulse[j] = OD_MAXI(0, (int)floorf(tmp));
      xy += x[j]*ypulse[j];
      yy += ypulse[j]*ypulse[j];
      i += ypulse[j];
    }
  }
  else OD_CLEAR(ypulse, n);

  /* Only use RDO on the last few pulses. This not only saves CPU, but using
     RDO on all pulses actually makes the results worse for reasons I don't
     fully understand. */
  rdo_pulses = 1 + k/4;
  /* Rough assumption for now, the last position costs about 3 bits more than
     the first. */
  delta_rate = 3./n;
  /* Search one pulse at a time */
  for (; i < k - rdo_pulses; i++) {
    int pos;
    float best_xy;
    float best_yy;
    pos = 0;
    best_xy = -10;
    best_yy = 1;
    for (j = 0; j < n; j++) {
      float tmp_xy;
      float tmp_yy;
      tmp_xy = xy + x[j];
      tmp_yy = yy + 2*ypulse[j] + 1;
      tmp_xy *= tmp_xy;
      if (j == 0 || tmp_xy*best_yy > best_xy*tmp_yy) {
        best_xy = tmp_xy;
        best_yy = tmp_yy;
        pos = j;
      }
    }
    xy = xy + x[pos];
    yy = yy + 2*ypulse[pos] + 1;
    ypulse[pos]++;
  }
  /* Search last pulses with RDO. Distortion is D = (x-y)^2 = x^2 - 2*x*y + y^2
     and since x^2 and y^2 are constant, we just maximize x*y, plus a
     lambda*rate term. Note that since x and y aren't normalized here,
     we need to divide by sqrt(x^2)*sqrt(y^2). */
  for (; i < k; i++) {
    float rsqrt_table[4];
    int rsqrt_table_size = 4;
    int pos;
    float best_cost;
    pos = 0;
    best_cost = -1e5;
    /*Fill the small rsqrt lookup table with inputs relative to yy.
      Specifically, the table of n values is filled with
       rsqrt(yy + 1), rsqrt(yy + 2 + 1) .. rsqrt(yy + 2*(n-1) + 1).*/
    od_fill_dynamic_rsqrt_table(rsqrt_table, rsqrt_table_size, yy);
    for (j = 0; j < n; j++) {
      float tmp_xy;
      float tmp_yy;
      tmp_xy = xy + x[j];
      /*Calculate rsqrt(yy + 2*ypulse[j] + 1) using an optimized method.*/
      tmp_yy = od_custom_rsqrt_dynamic_table(rsqrt_table, rsqrt_table_size,
       yy, ypulse[j]);
      tmp_xy = 2*norm_1*tmp_xy*tmp_yy - lambda*delta_rate*j;
      if (j == 0 || tmp_xy > best_cost) {
        best_cost = tmp_xy;
        pos = j;
      }
    }
    xy = xy + x[pos];
    yy = yy + 2*ypulse[pos] + 1;
    ypulse[pos]++;
  }
  for (i = 0; i < n; i++) {
    if (xcoeff[i] < 0) ypulse[i] = -ypulse[i];
  }
  return xy/(1e-100 + sqrtf(xx*yy));
}

/** Encodes the gain so that the return value increases with the
 * distance |x-ref|, so that we can encode a zero when x=ref. The
 * value x=0 is not covered because it is only allowed in the noref
 * case.
 *
 * @param [in]      x      quantized gain to encode
 * @param [in]      ref    quantized gain of the reference
 * @return                 interleave-encoded quantized gain value
 */
static int neg_interleave(int x, int ref) {
  if (x < ref) return -2*(x - ref) - 1;
  else if (x < 2*ref) return 2*(x - ref);
  else return x-1;
}

int od_vector_is_null(const od_coeff *x, int len) {
  int i;
  for (i = 0; i < len; i++) if (x[i]) return 0;
  return 1;
}

static float od_pvq_rate(int qg, int icgr, int theta, int ts,
 const od_adapt_ctx *adapt, const od_coeff *y0, int k, int n,
 int is_keyframe, int pli, int speed) {
  float rate;
  if (k == 0) rate = 0;
  else if (speed > 0) {
    int i;
    int sum;
    float f;
    /* Compute "center of mass" of the pulse vector. */
    sum = 0;
    for (i = 0; i < n - (theta != -1); i++) sum += i*abs(y0[i]);
    f = sum/(float)(k*n);
    /* Estimates the number of bits it will cost to encode K pulses in
       N dimensions based on hand-tuned fit for bitrate vs K, N and
       "center of mass". */
    rate = (1 + .4*f)*n*OD_LOG2(1 + OD_MAXF(0, log(n*2*(1*f + .025))*k/n)) + 3;
  }
  else {
    aom_writer w;
    od_pvq_codeword_ctx cd;
    int tell;
#if CONFIG_DAALA_EC
    od_ec_enc_init(&w.ec, 1000);
#else
# error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
    OD_COPY(&cd, &adapt->pvq.pvq_codeword_ctx, 1);
#if CONFIG_DAALA_EC
    tell = od_ec_enc_tell_frac(&w.ec);
#else
# error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
    aom_encode_pvq_codeword(&w, &cd, y0, n - (theta != -1), k);
#if CONFIG_DAALA_EC
    rate = (od_ec_enc_tell_frac(&w.ec)-tell)/8.;
    od_ec_enc_clear(&w.ec);
#else
# error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
  }
  if (qg > 0 && theta >= 0) {
    /* Approximate cost of entropy-coding theta */
    rate += .9*OD_LOG2(ts);
    /* Adding a cost to using the H/V pred because it's going to be off
       most of the time. Cost is optimized on subset1, while making
       sure we don't hurt the checkerboard image too much.
       FIXME: Do real RDO instead of this arbitrary cost. */
    if (is_keyframe && pli == 0) rate += 6;
    if (qg == icgr) rate -= .5;
  }
  return rate;
}

#define MAX_PVQ_ITEMS (20)
/* This stores the information about a PVQ search candidate, so we can sort
   based on K. */
typedef struct {
  int gain;
  int k;
  od_val32 qtheta;
  int theta;
  int ts;
  od_val32 qcg;
} pvq_search_item;

int items_compare(pvq_search_item *a, pvq_search_item *b) {
  /* Break ties in K with gain to ensure a stable sort.
     Otherwise, the order depends on qsort implementation. */
  return a->k == b->k ? a->gain - b->gain : a->k - b->k;
}

/** Perform PVQ quantization with prediction, trying several
 * possible gains and angles. See draft-valin-videocodec-pvq and
 * http://jmvalin.ca/slides/pvq.pdf for more details.
 *
 * @param [out]    out       coefficients after quantization
 * @param [in]     x0        coefficients before quantization
 * @param [in]     r0        reference, aka predicted coefficients
 * @param [in]     n         number of dimensions
 * @param [in]     q0        quantization step size
 * @param [out]    y         pulse vector (i.e. selected PVQ codevector)
 * @param [out]    itheta    angle between input and reference (-1 if noref)
 * @param [out]    max_theta maximum value of itheta that could have been
 * @param [out]    vk        total number of pulses
 * @param [in]     beta      per-band activity masking beta param
 * @param [out]    skip_diff distortion cost of skipping this block
 *                           (accumulated)
 * @param [in]     robust    make stream robust to error in the reference
 * @param [in]     is_keyframe whether we're encoding a keyframe
 * @param [in]     pli       plane index
 * @param [in]     adapt     probability adaptation context
 * @param [in]     qm        QM with magnitude compensation
 * @param [in]     qm_inv    Inverse of QM with magnitude compensation
 * @param [in] pvq_norm_lambda enc->pvq_norm_lambda for quantized RDO
 * @param [in]     speed     Make search faster by making approximations
 * @return         gain      index of the quatized gain
*/
static int pvq_theta(od_coeff *out, const od_coeff *x0, const od_coeff *r0,
 int n, int q0, od_coeff *y, int *itheta, int *max_theta, int *vk,
 od_val16 beta, float *skip_diff, int robust, int is_keyframe, int pli,
 const od_adapt_ctx *adapt, const int16_t *qm,
 const int16_t *qm_inv, float pvq_norm_lambda, int speed) {
  od_val32 g;
  od_val32 gr;
  od_coeff y_tmp[MAXN];
  int i;
  /* Number of pulses. */
  int k;
  /* Companded gain of x and reference, normalized to q. */
  od_val32 cg;
  od_val32 cgr;
  int icgr;
  int qg;
  /* Best RDO cost (D + lamdba*R) so far. */
  float best_cost;
  float dist0;
  /* Distortion (D) that corresponds to the best RDO cost. */
  float best_dist;
  float dist;
  /* Sign of Householder reflection. */
  int s;
  /* Dimension on which Householder reflects. */
  int m;
  od_val32 theta;
  float corr;
  int best_k;
  od_val32 best_qtheta;
  od_val32 gain_offset;
  int noref;
  float skip_dist;
  int cfl_enabled;
  int skip;
  float gain_weight;
  od_val16 x16[MAXN];
  od_val16 r16[MAXN];
  int xshift;
  int rshift;
  /* Give more weight to gain error when calculating the total distortion. */
  gain_weight = 1.0;
  OD_ASSERT(n > 1);
  corr = 0;
#if !defined(OD_FLOAT_PVQ)
  /* Shift needed to make x fit in 16 bits even after rotation.
     This shift value is not normative (it can be changed without breaking
     the bitstream) */
  xshift = OD_MAXI(0, od_vector_log_mag(x0, n) - 15);
  /* Shift needed to make the reference fit in 15 bits, so that the Householder
     vector can fit in 16 bits.
     This shift value *is* normative, and has to match the decoder. */
  rshift = OD_MAXI(0, od_vector_log_mag(r0, n) - 14);
#else
  xshift = 0;
  rshift = 0;
#endif
  for (i = 0; i < n; i++) {
#if defined(OD_FLOAT_PVQ)
    /*This is slightly different from the original float PVQ code,
       where the qm was applied in the accumulation in od_pvq_compute_gain and
       the vectors were od_coeffs, not od_val16 (i.e. float).*/
    x16[i] = x0[i]*(float)qm[i]*OD_QM_SCALE_1;
    r16[i] = r0[i]*(float)qm[i]*OD_QM_SCALE_1;
#else
    x16[i] = OD_SHR_ROUND(x0[i]*qm[i], OD_QM_SHIFT + xshift);
    r16[i] = OD_SHR_ROUND(r0[i]*qm[i], OD_QM_SHIFT + rshift);
#endif
    corr += OD_MULT16_16(x16[i], r16[i]);
  }
  cfl_enabled = is_keyframe && pli != 0 && !OD_DISABLE_CFL;
  cg  = od_pvq_compute_gain(x16, n, q0, &g, beta, xshift);
  cgr = od_pvq_compute_gain(r16, n, q0, &gr, beta, rshift);
  if (cfl_enabled) cgr = OD_CGAIN_SCALE;
  /* gain_offset is meant to make sure one of the quantized gains has
     exactly the same gain as the reference. */
#if defined(OD_FLOAT_PVQ)
  icgr = (int)floorf(.5 + cgr);
#else
  icgr = OD_SHR_ROUND(cgr, OD_CGAIN_SHIFT);
#endif
  gain_offset = cgr - OD_SHL(icgr, OD_CGAIN_SHIFT);
  /* Start search with null case: gain=0, no pulse. */
  qg = 0;
  dist = gain_weight*cg*cg*OD_CGAIN_SCALE_2;
  best_dist = dist;
  best_cost = dist + pvq_norm_lambda*od_pvq_rate(0, 0, -1, 0, adapt, NULL, 0,
   n, is_keyframe, pli, speed);
  noref = 1;
  best_k = 0;
  *itheta = -1;
  *max_theta = 0;
  OD_CLEAR(y, n);
  best_qtheta = 0;
  m = 0;
  s = 1;
  corr = corr/(1e-100 + g*(float)gr/OD_SHL(1, xshift + rshift));
  corr = OD_MAXF(OD_MINF(corr, 1.), -1.);
  if (is_keyframe) skip_dist = gain_weight*cg*cg*OD_CGAIN_SCALE_2;
  else {
    skip_dist = gain_weight*(cg - cgr)*(cg - cgr)
     + cgr*(float)cg*(2 - 2*corr);
    skip_dist *= OD_CGAIN_SCALE_2;
  }
  if (!is_keyframe) {
    /* noref, gain=0 isn't allowed, but skip is allowed. */
    od_val32 scgr;
    scgr = OD_MAXF(0,gain_offset);
    if (icgr == 0) {
      best_dist = gain_weight*(cg - scgr)*(cg - scgr)
       + scgr*(float)cg*(2 - 2*corr);
      best_dist *= OD_CGAIN_SCALE_2;
    }
    best_cost = best_dist + pvq_norm_lambda*od_pvq_rate(0, icgr, 0, 0, adapt,
     NULL, 0, n, is_keyframe, pli, speed);
    best_qtheta = 0;
    *itheta = 0;
    *max_theta = 0;
    noref = 0;
  }
  dist0 = best_dist;
  if (n <= OD_MAX_PVQ_SIZE && !od_vector_is_null(r0, n) && corr > 0) {
    od_val16 xr[MAXN];
    int gain_bound;
    int prev_k;
    pvq_search_item items[MAX_PVQ_ITEMS];
    int idx;
    int nitems;
    float cos_dist;
    idx = 0;
    gain_bound = OD_SHR(cg - gain_offset, OD_CGAIN_SHIFT);
    /* Perform theta search only if prediction is useful. */
    theta = OD_ROUND32(OD_THETA_SCALE*acos(corr));
    m = od_compute_householder(r16, n, gr, &s, rshift);
    od_apply_householder(xr, x16, r16, n);
    prev_k = 0;
    for (i = m; i < n - 1; i++) xr[i] = xr[i + 1];
    /* Compute all candidate PVQ searches within a reasonable range of gain
       and theta. */
    for (i = OD_MAXI(1, gain_bound - 1); i <= gain_bound + 1; i++) {
      int j;
      od_val32 qcg;
      int ts;
      int theta_lower;
      int theta_upper;
      /* Quantized companded gain */
      qcg = OD_SHL(i, OD_CGAIN_SHIFT) + gain_offset;
      /* Set angular resolution (in ra) to match the encoded gain */
      ts = od_pvq_compute_max_theta(qcg, beta);
      theta_lower = OD_MAXI(0, (int)floorf(.5 +
       theta*OD_THETA_SCALE_1*2/M_PI*ts) - 2);
      theta_upper = OD_MINI(ts - 1, (int)ceil(theta*OD_THETA_SCALE_1*2/M_PI*ts));
      /* Include the angles within a reasonable range. */
      for (j = theta_lower; j <= theta_upper; j++) {
        od_val32 qtheta;
        qtheta = od_pvq_compute_theta(j, ts);
        k = od_pvq_compute_k(qcg, j, qtheta, 0, n, beta, robust || is_keyframe);
        items[idx].gain = i;
        items[idx].theta = j;
        items[idx].k = k;
        items[idx].qcg = qcg;
        items[idx].qtheta = qtheta;
        items[idx].ts = ts;
        idx++;
        OD_ASSERT(idx < MAX_PVQ_ITEMS);
      }
    }
    nitems = idx;
    cos_dist = 0;
    /* Sort PVQ search candidates in ascending order of pulses K so that
       we can reuse all the previously searched pulses across searches. */
    qsort(items, nitems, sizeof(items[0]),
     (int (*)(const void *, const void *))items_compare);
    /* Search for the best gain/theta in order. */
    for (idx = 0; idx < nitems; idx++) {
      int j;
      od_val32 qcg;
      int ts;
      float cost;
      float dist_theta;
      float sin_prod;
      od_val32 qtheta;
      /* Quantized companded gain */
      qcg = items[idx].qcg;
      i = items[idx].gain;
      j = items[idx].theta;
      /* Set angular resolution (in ra) to match the encoded gain */
      ts = items[idx].ts;
      /* Search for the best angle within a reasonable range. */
      qtheta = items[idx].qtheta;
      k = items[idx].k;
      /* Compute the minimal possible distortion by not taking the PVQ
         cos_dist into account. */
      dist_theta = 2 - 2.*od_pvq_cos(theta - qtheta)*OD_TRIG_SCALE_1;
      dist = gain_weight*(qcg - cg)*(qcg - cg) + qcg*(float)cg*dist_theta;
      dist *= OD_CGAIN_SCALE_2;
      /* If we have no hope of beating skip (including a 1-bit worst-case
         penalty), stop now. */
      if (dist > dist0 + 1.0*pvq_norm_lambda && k != 0) continue;
      sin_prod = od_pvq_sin(theta)*OD_TRIG_SCALE_1*od_pvq_sin(qtheta)*
       OD_TRIG_SCALE_1;
      /* PVQ search, using a gain of qcg*cg*sin(theta)*sin(qtheta) since
         that's the factor by which cos_dist is multiplied to get the
         distortion metric. */
      if (k == 0) {
        cos_dist = 0;
        OD_CLEAR(y_tmp, n-1);
      }
      else if (k != prev_k) {
        cos_dist = pvq_search_rdo_float(xr, n - 1, k, y_tmp,
         qcg*(float)cg*sin_prod*OD_CGAIN_SCALE_2, pvq_norm_lambda, prev_k);
      }
      prev_k = k;
      /* See Jmspeex' Journal of Dubious Theoretical Results. */
      dist_theta = 2 - 2.*od_pvq_cos(theta - qtheta)*OD_TRIG_SCALE_1
       + sin_prod*(2 - 2*cos_dist);
      dist = gain_weight*(qcg - cg)*(qcg - cg) + qcg*(float)cg*dist_theta;
      dist *= OD_CGAIN_SCALE_2;
      /* Do approximate RDO. */
      cost = dist + pvq_norm_lambda*od_pvq_rate(i, icgr, j, ts, adapt, y_tmp,
       k, n, is_keyframe, pli, speed);
      if (cost < best_cost) {
        best_cost = cost;
        best_dist = dist;
        qg = i;
        best_k = k;
        best_qtheta = qtheta;
        *itheta = j;
        *max_theta = ts;
        noref = 0;
        OD_COPY(y, y_tmp, n - 1);
      }
    }
  }
  /* Don't bother with no-reference version if there's a reasonable
     correlation. The only exception is luma on a keyframe because
     H/V prediction is unreliable. */
  if (n <= OD_MAX_PVQ_SIZE &&
   ((is_keyframe && pli == 0) || corr < .5
   || cg < (od_val32)(OD_SHL(2, OD_CGAIN_SHIFT)))) {
    int gain_bound;
    int prev_k;
    gain_bound = OD_SHR(cg, OD_CGAIN_SHIFT);
    prev_k = 0;
    /* Search for the best gain (haven't determined reasonable range yet). */
    for (i = OD_MAXI(1, gain_bound); i <= gain_bound + 1; i++) {
      float cos_dist;
      float cost;
      od_val32 qcg;
      qcg = OD_SHL(i, OD_CGAIN_SHIFT);
      k = od_pvq_compute_k(qcg, -1, -1, 1, n, beta, robust || is_keyframe);
      /* Compute the minimal possible distortion by not taking the PVQ
         cos_dist into account. */
      dist = gain_weight*(qcg - cg)*(qcg - cg);
      dist *= OD_CGAIN_SCALE_2;
      if (dist > dist0 && k != 0) continue;
      cos_dist = pvq_search_rdo_float(x16, n, k, y_tmp,
       qcg*(float)cg*OD_CGAIN_SCALE_2, pvq_norm_lambda, prev_k);
      prev_k = k;
      /* See Jmspeex' Journal of Dubious Theoretical Results. */
      dist = gain_weight*(qcg - cg)*(qcg - cg)
       + qcg*(float)cg*(2 - 2*cos_dist);
      dist *= OD_CGAIN_SCALE_2;
      /* Do approximate RDO. */
      cost = dist + pvq_norm_lambda*od_pvq_rate(i, 0, -1, 0, adapt, y_tmp, k,
       n, is_keyframe, pli, speed);
      if (cost <= best_cost) {
        best_cost = cost;
        best_dist = dist;
        qg = i;
        noref = 1;
        best_k = k;
        *itheta = -1;
        *max_theta = 0;
        OD_COPY(y, y_tmp, n);
      }
    }
  }
  k = best_k;
  theta = best_qtheta;
  skip = 0;
  if (noref) {
    if (qg == 0) skip = OD_PVQ_SKIP_ZERO;
  }
  else {
    if (!is_keyframe && qg == 0) {
      skip = (icgr ? OD_PVQ_SKIP_ZERO : OD_PVQ_SKIP_COPY);
    }
    if (qg == icgr && *itheta == 0 && !cfl_enabled) skip = OD_PVQ_SKIP_COPY;
  }
  /* Synthesize like the decoder would. */
  if (skip) {
    if (skip == OD_PVQ_SKIP_COPY) OD_COPY(out, r0, n);
    else OD_CLEAR(out, n);
  }
  else {
    if (noref) gain_offset = 0;
    g = od_gain_expand(OD_SHL(qg, OD_CGAIN_SHIFT) + gain_offset, q0, beta);
    od_pvq_synthesis_partial(out, y, r16, n, noref, g, theta, m, s,
     qm_inv);
  }
  *vk = k;
  *skip_diff += skip_dist - best_dist;
  /* Encode gain differently depending on whether we use prediction or not.
     Special encoding on inter frames where qg=0 is allowed for noref=0
     but not noref=1.*/
  if (is_keyframe) return noref ? qg : neg_interleave(qg, icgr);
  else return noref ? qg - 1 : neg_interleave(qg + 1, icgr + 1);
}

/** Encodes a single vector of integers (eg, a partition within a
 *  coefficient block) using PVQ
 *
 * @param [in,out] w          multi-symbol entropy encoder
 * @param [in]     qg         quantized gain
 * @param [in]     theta      quantized post-prediction theta
 * @param [in]     max_theta  maximum possible quantized theta value
 * @param [in]     in         coefficient vector to code
 * @param [in]     n          number of coefficients in partition
 * @param [in]     k          number of pulses in partition
 * @param [in,out] model      entropy encoder state
 * @param [in,out] adapt      adaptation context
 * @param [in,out] exg        ExQ16 expectation of gain value
 * @param [in,out] ext        ExQ16 expectation of theta value
 * @param [in]     nodesync   do not use info that depend on the reference
 * @param [in]     cdf_ctx    selects which cdf context to use
 * @param [in]     is_keyframe whether we're encoding a keyframe
 * @param [in]     code_skip  whether the "skip rest" flag is allowed
 * @param [in]     skip_rest  when set, we skip all higher bands
 * @param [in]     encode_flip whether we need to encode the CfL flip flag now
 * @param [in]     flip       value of the CfL flip flag
 */
void pvq_encode_partition(aom_writer *w,
                                 int qg,
                                 int theta,
                                 int max_theta,
                                 const od_coeff *in,
                                 int n,
                                 int k,
                                 generic_encoder model[3],
                                 od_adapt_ctx *adapt,
                                 int *exg,
                                 int *ext,
                                 int nodesync,
                                 int cdf_ctx,
                                 int is_keyframe,
                                 int code_skip,
                                 int skip_rest,
                                 int encode_flip,
                                 int flip) {
  int noref;
  int id;
  noref = (theta == -1);
  id = (qg > 0) + 2*OD_MINI(theta + 1,3) + 8*code_skip*skip_rest;
  if (is_keyframe) {
    OD_ASSERT(id != 8);
    if (id >= 8) id--;
  }
  else {
    OD_ASSERT(id != 10);
    if (id >= 10) id--;
  }
  /* Jointly code gain, theta and noref for small values. Then we handle
     larger gain and theta values. For noref, theta = -1. */
  aom_encode_cdf_adapt(w, id, &adapt->pvq.pvq_gaintheta_cdf[cdf_ctx][0],
   8 + 7*code_skip, adapt->pvq.pvq_gaintheta_increment);
  if (encode_flip) {
    /* We could eventually do some smarter entropy coding here, but it would
       have to be good enough to overcome the overhead of the entropy coder.
       An early attempt using a "toogle" flag with simple adaptation wasn't
       worth the trouble. */
    aom_write_bit(w, flip);
  }
  if (qg > 0) {
    int tmp;
    tmp = *exg;
    generic_encode(w, &model[!noref], qg - 1, -1, &tmp, 2);
    OD_IIR_DIADIC(*exg, qg << 16, 2);
  }
  if (theta > 1 && (nodesync || max_theta > 3)) {
    int tmp;
    tmp = *ext;
    generic_encode(w, &model[2], theta - 2, nodesync ? -1 : max_theta - 3,
     &tmp, 2);
    OD_IIR_DIADIC(*ext, theta << 16, 2);
  }
  aom_encode_pvq_codeword(w, &adapt->pvq.pvq_codeword_ctx, in,
   n - (theta != -1), k);
}

/** Quantizes a scalar with rate-distortion optimization (RDO)
 * @param [in] x      unquantized value
 * @param [in] q      quantization step size
 * @param [in] delta0 rate increase for encoding a 1 instead of a 0
 * @param [in] pvq_norm_lambda enc->pvq_norm_lambda for quantized RDO
 * @retval quantized value
 */
int od_rdo_quant(od_coeff x, int q, float delta0, float pvq_norm_lambda) {
  int n;
  /* Optimal quantization threshold is 1/2 + lambda*delta_rate/2. See
     Jmspeex' Journal of Dubious Theoretical Results for details. */
  n = OD_DIV_R0(abs(x), q);
  if ((float)abs(x)/q < (float)n/2 + pvq_norm_lambda*delta0/(2*n)) {
    return 0;
  }
  else {
    return OD_DIV_R0(x, q);
  }
}

#if OD_SIGNAL_Q_SCALING
void od_encode_quantizer_scaling(daala_enc_ctx *enc, int q_scaling,
 int sbx, int sby, int skip) {
  int nhsb;
  OD_ASSERT(skip == !!skip);
  nhsb = enc->state.nhsb;
  OD_ASSERT(sbx < nhsb);
  OD_ASSERT(sby < enc->state.nvsb);
  OD_ASSERT(!skip || q_scaling == 0);
  enc->state.sb_q_scaling[sby*nhsb + sbx] = q_scaling;
  if (!skip) {
    int above;
    int left;
    /* use value from neighbour if possible, otherwise use 0 */
    above = sby > 0 ? enc->state.sb_q_scaling[(sby - 1)*enc->state.nhsb + sbx]
     : 0;
    left = sbx > 0 ? enc->state.sb_q_scaling[sby*enc->state.nhsb + (sbx - 1)]
     : 0;
    aom_encode_cdf_adapt(&enc->w, q_scaling,
     enc->state.adapt.q_cdf[above + left*4], 4,
     enc->state.adapt.q_increment);
  }
}
#endif

/** Encode a coefficient block (excepting DC) using PVQ
 *
 * @param [in,out] enc     daala encoder context
 * @param [in]     ref     'reference' (prediction) vector
 * @param [in]     in      coefficient block to quantize and encode
 * @param [out]    out     quantized coefficient block
 * @param [in]     q0      scale/quantizer
 * @param [in]     pli     plane index
 * @param [in]     bs      log of the block size minus two
 * @param [in]     beta    per-band activity masking beta param
 * @param [in]     robust  make stream robust to error in the reference
 * @param [in]     is_keyframe whether we're encoding a keyframe
 * @param [in]     q_scaling scaling factor to apply to quantizer
 * @param [in]     bx      x-coordinate of this block
 * @param [in]     by      y-coordinate of this block
 * @param [in]     qm      QM with magnitude compensation
 * @param [in]     qm_inv  Inverse of QM with magnitude compensation
 * @param [in]     speed   Make search faster by making approximations
 * @param [in]     pvq_info If null, conisdered as RDO search mode
 * @return         Returns block skip info indicating whether DC/AC are coded.
 *                 bit0: DC is coded, bit1: AC is coded (1 means coded)
 *
 */
PVQ_SKIP_TYPE od_pvq_encode(daala_enc_ctx *enc,
                   od_coeff *ref,
                   const od_coeff *in,
                   od_coeff *out,
                   int q_dc,
                   int q_ac,
                   int pli,
                   int bs,
                   const od_val16 *beta,
                   int robust,
                   int is_keyframe,
                   int q_scaling,
                   int bx,
                   int by,
                   const int16_t *qm,
                   const int16_t *qm_inv,
                   int speed,
                   PVQ_INFO *pvq_info){
  int theta[PVQ_MAX_PARTITIONS];
  int max_theta[PVQ_MAX_PARTITIONS];
  int qg[PVQ_MAX_PARTITIONS];
  int k[PVQ_MAX_PARTITIONS];
  od_coeff y[OD_TXSIZE_MAX*OD_TXSIZE_MAX];
  int *exg;
  int *ext;
  int nb_bands;
  int i;
  const int *off;
  int size[PVQ_MAX_PARTITIONS];
  generic_encoder *model;
  float skip_diff;
  int tell;
  uint16_t *skip_cdf;
  od_rollback_buffer buf;
  int dc_quant;
  int flip;
  int cfl_encoded;
  int skip_rest;
  int skip_dir;
  int skip_theta_value;
  const unsigned char *pvq_qm;
  float dc_rate;
  int use_masking;
  PVQ_SKIP_TYPE ac_dc_coded;
#if !OD_SIGNAL_Q_SCALING
  OD_UNUSED(q_scaling);
  OD_UNUSED(bx);
  OD_UNUSED(by);
#endif

  use_masking = enc->use_activity_masking;

  if (use_masking)
    pvq_qm = &enc->state.pvq_qm_q4[pli][0];
  else
    pvq_qm = 0;

  exg = &enc->state.adapt.pvq.pvq_exg[pli][bs][0];
  ext = enc->state.adapt.pvq.pvq_ext + bs*PVQ_MAX_PARTITIONS;
  skip_cdf = enc->state.adapt.skip_cdf[2*bs + (pli != 0)];
  model = enc->state.adapt.pvq.pvq_param_model;
  nb_bands = OD_BAND_OFFSETS[bs][0];
  off = &OD_BAND_OFFSETS[bs][1];

  if (use_masking)
    dc_quant = OD_MAXI(1, q_dc * pvq_qm[od_qm_get_index(bs, 0)] >> 4);
  else
    dc_quant = OD_MAXI(1, q_dc);

  tell = 0;
  for (i = 0; i < nb_bands; i++) size[i] = off[i+1] - off[i];
  skip_diff = 0;
  flip = 0;
  /*If we are coding a chroma block of a keyframe, we are doing CfL.*/
  if (pli != 0 && is_keyframe) {
    od_val32 xy;
    xy = 0;
    /*Compute the dot-product of the first band of chroma with the luma ref.*/
    for (i = off[0]; i < off[1]; i++) {
#if defined(OD_FLOAT_PVQ)
      xy += ref[i]*(float)qm[i]*OD_QM_SCALE_1*
       (float)in[i]*(float)qm[i]*OD_QM_SCALE_1;
#else
      od_val32 rq;
      od_val32 inq;
      rq = ref[i]*qm[i];
      inq = in[i]*qm[i];
      xy += OD_SHR(rq*(int64_t)inq, OD_SHL(OD_QM_SHIFT + OD_CFL_FLIP_SHIFT,
       1));
#endif
    }
    /*If cos(theta) < 0, then |theta| > pi/2 and we should negate the ref.*/
    if (xy < 0) {
      flip = 1;
      for(i = off[0]; i < off[nb_bands]; i++) ref[i] = -ref[i];
    }
  }
  for (i = 0; i < nb_bands; i++) {
    int q;

    if (use_masking)
      q = OD_MAXI(1, q_ac * pvq_qm[od_qm_get_index(bs, i + 1)] >> 4);
    else
      q = OD_MAXI(1, q_ac);

    qg[i] = pvq_theta(out + off[i], in + off[i], ref + off[i], size[i],
     q, y + off[i], &theta[i], &max_theta[i],
     &k[i], beta[i], &skip_diff, robust, is_keyframe, pli, &enc->state.adapt,
     qm + off[i], qm_inv + off[i], enc->pvq_norm_lambda, speed);
  }
  od_encode_checkpoint(enc, &buf);
  if (is_keyframe) out[0] = 0;
  else {
    int n;
    n = OD_DIV_R0(abs(in[0] - ref[0]), dc_quant);
    if (n == 0) {
      out[0] = 0;
#if PVQ_CHROMA_RD
    } else if (pli == 0) {
#else
    } else {
#endif
      int tell2;
      od_rollback_buffer dc_buf;

      dc_rate = -OD_LOG2((float)(skip_cdf[3] - skip_cdf[2])/
       (float)(skip_cdf[2] - skip_cdf[1]));
      dc_rate += 1;

#if CONFIG_DAALA_EC
      tell2 = od_ec_enc_tell_frac(&enc->w.ec);
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
      od_encode_checkpoint(enc, &dc_buf);
      generic_encode(&enc->w, &enc->state.adapt.model_dc[pli],
       n - 1, -1, &enc->state.adapt.ex_dc[pli][bs][0], 2);
#if CONFIG_DAALA_EC
      tell2 = od_ec_enc_tell_frac(&enc->w.ec) - tell2;
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
      dc_rate += tell2/8.0;
      od_encode_rollback(enc, &dc_buf);

      out[0] = od_rdo_quant(in[0] - ref[0], dc_quant, dc_rate,
       enc->pvq_norm_lambda);
    }
  }
#if CONFIG_DAALA_EC
  tell = od_ec_enc_tell_frac(&enc->w.ec);
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
  /* Code as if we're not skipping. */
  aom_encode_cdf_adapt(&enc->w, 2 + (out[0] != 0), skip_cdf,
   4, enc->state.adapt.skip_increment);
  ac_dc_coded = AC_CODED + (out[0] != 0);
#if OD_SIGNAL_Q_SCALING
  if (bs == OD_TXSIZES - 1 && pli == 0) {
    od_encode_quantizer_scaling(enc, q_scaling, bx >> (OD_TXSIZES - 1),
     by >> (OD_TXSIZES - 1), 0);
  }
#endif
  cfl_encoded = 0;
  skip_rest = 1;
  skip_theta_value = is_keyframe ? -1 : 0;
  for (i = 1; i < nb_bands; i++) {
    if (theta[i] != skip_theta_value || qg[i]) skip_rest = 0;
  }
  skip_dir = 0;
  if (nb_bands > 1) {
    for (i = 0; i < 3; i++) {
      int j;
      int tmp;
      tmp = 1;
      // ToDo(yaowu): figure out better stop condition without gcc warning.
      for (j = i + 1; j < nb_bands && j < PVQ_MAX_PARTITIONS; j += 3) {
        if (theta[j] != skip_theta_value || qg[j]) tmp = 0;
      }
      skip_dir |= tmp << i;
    }
  }
  if (theta[0] == skip_theta_value && qg[0] == 0 && skip_rest) nb_bands = 0;

  /* NOTE: There was no other better place to put this function. */
  if (pvq_info)
    av1_store_pvq_enc_info(pvq_info, qg, theta, max_theta, k,
      y, nb_bands, off, size,
      skip_rest, skip_dir, bs);

  for (i = 0; i < nb_bands; i++) {
    int encode_flip;
    /* Encode CFL flip bit just after the first time it's used. */
    encode_flip = pli != 0 && is_keyframe && theta[i] != -1 && !cfl_encoded;
    if (i == 0 || (!skip_rest && !(skip_dir & (1 << ((i - 1)%3))))) {
      pvq_encode_partition(&enc->w, qg[i], theta[i], max_theta[i], y + off[i],
       size[i], k[i], model, &enc->state.adapt, exg + i, ext + i,
       robust || is_keyframe, (pli != 0)*OD_TXSIZES*PVQ_MAX_PARTITIONS
       + bs*PVQ_MAX_PARTITIONS + i, is_keyframe, i == 0 && (i < nb_bands - 1),
       skip_rest, encode_flip, flip);
    }
    if (i == 0 && !skip_rest && bs > 0) {
      aom_encode_cdf_adapt(&enc->w, skip_dir,
       &enc->state.adapt.pvq.pvq_skip_dir_cdf[(pli != 0) + 2*(bs - 1)][0], 7,
       enc->state.adapt.pvq.pvq_skip_dir_increment);
    }
    if (encode_flip) cfl_encoded = 1;
  }
#if CONFIG_DAALA_EC
  tell = od_ec_enc_tell_frac(&enc->w.ec) - tell;
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
  /* Account for the rate of skipping the AC, based on the same DC decision
     we made when trying to not skip AC. */
  {
    float skip_rate;
    if (out[0] != 0) {
      skip_rate = -OD_LOG2((skip_cdf[1] - skip_cdf[0])/
     (float)skip_cdf[3]);
    }
    else {
      skip_rate = -OD_LOG2(skip_cdf[0]/
     (float)skip_cdf[3]);
    }
    tell -= (int)floorf(.5+8*skip_rate);
  }
  if (nb_bands == 0 || skip_diff <= enc->pvq_norm_lambda/8*tell) {
    if (is_keyframe) out[0] = 0;
    else {
      int n;
      n = OD_DIV_R0(abs(in[0] - ref[0]), dc_quant);
      if (n == 0) {
        out[0] = 0;
#if PVQ_CHROMA_RD
      } else if (pli == 0) {
#else
      } else {
#endif
        int tell2;
        od_rollback_buffer dc_buf;

        dc_rate = -OD_LOG2((float)(skip_cdf[1] - skip_cdf[0])/
         (float)skip_cdf[0]);
        dc_rate += 1;

#if CONFIG_DAALA_EC
        tell2 = od_ec_enc_tell_frac(&enc->w.ec);
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
        od_encode_checkpoint(enc, &dc_buf);
        generic_encode(&enc->w, &enc->state.adapt.model_dc[pli],
         n - 1, -1, &enc->state.adapt.ex_dc[pli][bs][0], 2);
#if CONFIG_DAALA_EC
        tell2 = od_ec_enc_tell_frac(&enc->w.ec) - tell2;
#else
#error "CONFIG_PVQ currently requires CONFIG_DAALA_EC."
#endif
        dc_rate += tell2/8.0;
        od_encode_rollback(enc, &dc_buf);

        out[0] = od_rdo_quant(in[0] - ref[0], dc_quant, dc_rate,
         enc->pvq_norm_lambda);
      }
    }
    /* We decide to skip, roll back everything as it was before. */
    od_encode_rollback(enc, &buf);
    aom_encode_cdf_adapt(&enc->w, out[0] != 0, skip_cdf,
     4, enc->state.adapt.skip_increment);
    ac_dc_coded = (out[0] != 0);
#if OD_SIGNAL_Q_SCALING
    if (bs == OD_TXSIZES - 1 && pli == 0) {
      int skip;
      skip = out[0] == 0;
      if (skip) {
        q_scaling = 0;
      }
      od_encode_quantizer_scaling(enc, q_scaling, bx >> (OD_TXSIZES - 1),
       by >> (OD_TXSIZES - 1), skip);
    }
#endif
    if (is_keyframe) for (i = 1; i < 1 << (2*bs + 4); i++) out[i] = 0;
    else for (i = 1; i < 1 << (2*bs + 4); i++) out[i] = ref[i];
  }
  if (pvq_info)
    pvq_info->ac_dc_coded = ac_dc_coded;
  return ac_dc_coded;
}
