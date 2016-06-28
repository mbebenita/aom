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

#ifndef AOM_DSP_ANSWRITER_H_
#define AOM_DSP_ANSWRITER_H_
// A uABS and rANS encoder implementation of Asymmetric Numeral Systems
// http://arxiv.org/abs/1311.2540v2

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/ans.h"
#include "aom_dsp/prob.h"
#include "aom_ports/mem_ops.h"

#define ANS_DIV(dividend, divisor) ((dividend) / (divisor))

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct AnsCoder {
  uint8_t *buf;
  int buf_offset;
  uint32_t state;
};

static INLINE void ans_write_init(struct AnsCoder *const ans,
                                  uint8_t *const buf) {
  ans->buf = buf;
  ans->buf_offset = 0;
  ans->state = L_BASE;
}

static INLINE int ans_write_end(struct AnsCoder *const ans) {
  uint32_t state;
  assert(ans->state >= L_BASE);
  assert(ans->state < L_BASE * IO_BASE);
  state = ans->state - L_BASE;
  if (state < (1 << 6)) {
    ans->buf[ans->buf_offset] = (0x00 << 6) + state;
    return ans->buf_offset + 1;
  } else if (state < (1 << 14)) {
    mem_put_le16(ans->buf + ans->buf_offset, (0x01 << 14) + state);
    return ans->buf_offset + 2;
  } else if (state < (1 << 22)) {
    mem_put_le24(ans->buf + ans->buf_offset, (0x02 << 22) + state);
    return ans->buf_offset + 3;
  } else {
    assert(0 && "State is too large to be serialized");
    return ans->buf_offset;
  }
}

// uABS with normalization
static INLINE void uabs_write(struct AnsCoder *ans, int val, AnsP8 p0) {
  AnsP8 p = ANS_P8_PRECISION - p0;
  const unsigned l_s = val ? p : p0;
  while (ans->state >= L_BASE / ANS_P8_PRECISION * IO_BASE * l_s) {
    ans->buf[ans->buf_offset++] = ans->state % IO_BASE;
    ans->state /= IO_BASE;
  }
  if (!val)
    ans->state = ANS_DIV(ans->state * ANS_P8_PRECISION, p0);
  else
    ans->state = ANS_DIV((ans->state + 1) * ANS_P8_PRECISION + p - 1, p) - 1;
}

struct rans_sym {
  AnsP10 prob;
  AnsP10 cum_prob;  // not-inclusive
};

// rANS with normalization
// sym->prob takes the place of l_s from the paper
// ANS_P10_PRECISION is m
static INLINE void rans_write(struct AnsCoder *ans,
                              const struct rans_sym *const sym) {
  const AnsP10 p = sym->prob;
  while (ans->state >= L_BASE / RANS_PRECISION * IO_BASE * p) {
    ans->buf[ans->buf_offset++] = ans->state % IO_BASE;
    ans->state /= IO_BASE;
  }
  ans->state =
      (ans->state / p) * RANS_PRECISION + ans->state % p + sym->cum_prob;
}

#undef ANS_DIV
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_DSP_ANSWRITER_H_
