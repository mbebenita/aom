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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

using libaom_test::ACMRandom;

namespace {
const int kNumCoeffs = 16;
typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*FhtFunc)(const int16_t *in, tran_low_t *out, int stride,
                        int tx_type);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        int tx_type);

typedef std::tr1::tuple<FdctFunc, IdctFunc, int, aom_bit_depth_t> Dct4x4Param;
typedef std::tr1::tuple<FhtFunc, IhtFunc, int, aom_bit_depth_t> Ht4x4Param;

void fdct4x4_ref(const int16_t *in, tran_low_t *out, int stride, int tx_type) {
  aom_fdct4x4_c(in, out, stride);
}

void fht4x4_ref(const int16_t *in, tran_low_t *out, int stride, int tx_type) {
  av1_fht4x4_c(in, out, stride, tx_type);
}

void fwht4x4_ref(const int16_t *in, tran_low_t *out, int stride, int tx_type) {
  av1_fwht4x4_c(in, out, stride);
}

#if CONFIG_AOM_HIGHBITDEPTH
void idct4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_idct4x4_16_add_c(in, out, stride, 10);
}

void idct4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_idct4x4_16_add_c(in, out, stride, 12);
}

void iht4x4_10(const tran_low_t *in, uint8_t *out, int stride, int tx_type) {
  av1_highbd_iht4x4_16_add_c(in, out, stride, tx_type, 10);
}

void iht4x4_12(const tran_low_t *in, uint8_t *out, int stride, int tx_type) {
  av1_highbd_iht4x4_16_add_c(in, out, stride, tx_type, 12);
}

void iwht4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_iwht4x4_16_add_c(in, out, stride, 10);
}

void iwht4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_iwht4x4_16_add_c(in, out, stride, 12);
}

#if HAVE_SSE2
void idct4x4_10_sse2(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_idct4x4_16_add_sse2(in, out, stride, 10);
}

void idct4x4_12_sse2(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_idct4x4_16_add_sse2(in, out, stride, 12);
}
#endif  // HAVE_SSE2
#endif  // CONFIG_AOM_HIGHBITDEPTH

class Trans4x4TestBase {
 public:
  virtual ~Trans4x4TestBase() {}

 protected:
  virtual void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) = 0;

  virtual void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) = 0;

  void RunAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    uint32_t max_error = 0;
    int64_t total_error = 0;
    const int count_test_block = 10000;
    for (int i = 0; i < count_test_block; ++i) {
      DECLARE_ALIGNED(16, int16_t, test_input_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, tran_low_t, test_temp_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_AOM_HIGHBITDEPTH
      DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          test_input_block[j] = src[j] - dst[j];
#if CONFIG_AOM_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          test_input_block[j] = src16[j] - dst16[j];
#endif
        }
      }

      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_temp_block, pitch_));
      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));
#if CONFIG_AOM_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_AOM_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        ASSERT_EQ(AOM_BITS_8, bit_depth_);
        const int diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        if (max_error < error) max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(static_cast<uint32_t>(limit), max_error)
        << "Error: 4x4 FHT/IHT has an individual round trip error > " << limit;

    EXPECT_GE(count_test_block * limit, total_error)
        << "Error: 4x4 FHT/IHT has average round trip error > " << limit
        << " per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED(16, int16_t, input_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j)
        input_block[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);

      fwd_txfm_ref(input_block, output_ref_block, pitch_, tx_type_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_EQ(output_block[j], output_ref_block[j]);
    }
  }

  void RunMemCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        input_extreme_block[j] = rnd.Rand8() % 2 ? mask_ : -mask_;
      }
      if (i == 0) {
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = mask_;
      } else if (i == 1) {
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = -mask_;
      }

      fwd_txfm_ref(input_extreme_block, output_ref_block, pitch_, tx_type_);
      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(input_extreme_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j) {
        EXPECT_EQ(output_block[j], output_ref_block[j]);
        EXPECT_GE(4 * DCT_MAX_VALUE << (bit_depth_ - 8), abs(output_block[j]))
            << "Error: 4x4 FDCT has coefficient larger than 4*DCT_MAX_VALUE";
      }
    }
  }

  void RunInvAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, in[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_AOM_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          in[j] = src[j] - dst[j];
#if CONFIG_AOM_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          in[j] = src16[j] - dst16[j];
#endif
        }
      }

      fwd_txfm_ref(in, coeff, pitch_, tx_type_);

      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));
#if CONFIG_AOM_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_AOM_HIGHBITDEPTH
        const uint32_t diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const uint32_t diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        EXPECT_GE(static_cast<uint32_t>(limit), error)
            << "Error: 4x4 IDCT has error " << error << " at index " << j;
      }
    }
  }

  int pitch_;
  int tx_type_;
  FhtFunc fwd_txfm_ref;
  aom_bit_depth_t bit_depth_;
  int mask_;
};

class Trans4x4DCT : public Trans4x4TestBase,
                    public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_ = GET_PARAM(2);
    pitch_ = 4;
    fwd_txfm_ref = fdct4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4DCT, AccuracyCheck) { RunAccuracyCheck(1); }

TEST_P(Trans4x4DCT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4DCT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4DCT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

class Trans4x4HT : public Trans4x4TestBase,
                   public ::testing::TestWithParam<Ht4x4Param> {
 public:
  virtual ~Trans4x4HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_ = GET_PARAM(2);
    pitch_ = 4;
    fwd_txfm_ref = fht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride, tx_type_);
  }

  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, tx_type_);
  }

  FhtFunc fwd_txfm_;
  IhtFunc inv_txfm_;
};

TEST_P(Trans4x4HT, AccuracyCheck) { RunAccuracyCheck(1); }

TEST_P(Trans4x4HT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4HT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4HT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

class Trans4x4WHT : public Trans4x4TestBase,
                    public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4WHT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_ = GET_PARAM(2);
    pitch_ = 4;
    fwd_txfm_ref = fwht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4WHT, AccuracyCheck) { RunAccuracyCheck(0); }

TEST_P(Trans4x4WHT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4WHT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4WHT, InvAccuracyCheck) { RunInvAccuracyCheck(0); }
using std::tr1::make_tuple;

#if CONFIG_AOM_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&aom_highbd_fdct4x4_c, &idct4x4_10, 0, AOM_BITS_10),
        make_tuple(&aom_highbd_fdct4x4_c, &idct4x4_12, 0, AOM_BITS_12),
        make_tuple(&aom_fdct4x4_c, &aom_idct4x4_16_add_c, 0, AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(C, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_c,
                                                     &aom_idct4x4_16_add_c, 0,
                                                     AOM_BITS_8)));
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if CONFIG_AOM_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_10, 0, AOM_BITS_10),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_10, 1, AOM_BITS_10),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_10, 2, AOM_BITS_10),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_10, 3, AOM_BITS_10),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_12, 0, AOM_BITS_12),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_12, 1, AOM_BITS_12),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_12, 2, AOM_BITS_12),
        make_tuple(&av1_highbd_fht4x4_c, &iht4x4_12, 3, AOM_BITS_12),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 3, AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, 3, AOM_BITS_8)));
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if CONFIG_AOM_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4WHT,
    ::testing::Values(
        make_tuple(&av1_highbd_fwht4x4_c, &iwht4x4_10, 0, AOM_BITS_10),
        make_tuple(&av1_highbd_fwht4x4_c, &iwht4x4_12, 0, AOM_BITS_12),
        make_tuple(&av1_fwht4x4_c, &aom_iwht4x4_16_add_c, 0, AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(C, Trans4x4WHT,
                        ::testing::Values(make_tuple(&av1_fwht4x4_c,
                                                     &aom_iwht4x4_16_add_c, 0,
                                                     AOM_BITS_8)));
#endif  // CONFIG_AOM_HIGHBITDEPTH

#if HAVE_NEON_ASM && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(NEON, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_c,
                                                     &aom_idct4x4_16_add_neon,
                                                     0, AOM_BITS_8)));
#endif  // HAVE_NEON_ASM && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_NEON && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    NEON, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon, 3, AOM_BITS_8)));
#endif  // HAVE_NEON && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if CONFIG_USE_X86INC && HAVE_SSE2 && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4WHT,
    ::testing::Values(
        make_tuple(&av1_fwht4x4_sse2, &aom_iwht4x4_16_add_c, 0, AOM_BITS_8),
        make_tuple(&av1_fwht4x4_c, &aom_iwht4x4_16_add_sse2, 0, AOM_BITS_8)));
#endif

#if HAVE_SSE2 && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(SSE2, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_sse2,
                                                     &aom_idct4x4_16_add_sse2,
                                                     0, AOM_BITS_8)));
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2, 3, AOM_BITS_8)));
#endif  // HAVE_SSE2 && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_SSE2 && CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&aom_highbd_fdct4x4_c, &idct4x4_10_sse2, 0, AOM_BITS_10),
        make_tuple(&aom_highbd_fdct4x4_sse2, &idct4x4_10_sse2, 0, AOM_BITS_10),
        make_tuple(&aom_highbd_fdct4x4_c, &idct4x4_12_sse2, 0, AOM_BITS_12),
        make_tuple(&aom_highbd_fdct4x4_sse2, &idct4x4_12_sse2, 0, AOM_BITS_12),
        make_tuple(&aom_fdct4x4_sse2, &aom_idct4x4_16_add_c, 0, AOM_BITS_8)));

INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c, 3, AOM_BITS_8)));
#endif  // HAVE_SSE2 && CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_MSA && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(MSA, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_msa,
                                                     &aom_idct4x4_16_add_msa, 0,
                                                     AOM_BITS_8)));
INSTANTIATE_TEST_CASE_P(
    MSA, Trans4x4HT,
    ::testing::Values(
        make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa, 0, AOM_BITS_8),
        make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa, 1, AOM_BITS_8),
        make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa, 2, AOM_BITS_8),
        make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa, 3, AOM_BITS_8)));
#endif  // HAVE_MSA && !CONFIG_AOM_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
}  // namespace
