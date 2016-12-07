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
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "./aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/postproc.h"
#include "aom_mem/aom_mem.h"

namespace {

// TODO(jimbankoski): make width and height integers not unsigned.
typedef void (*AddNoiseFunc)(unsigned char *start, char *noise,
                             char blackclamp[16], char whiteclamp[16],
                             char bothclamp[16], unsigned int width,
                             unsigned int height, int pitch);

class AddNoiseTest : public ::testing::TestWithParam<AddNoiseFunc> {
 public:
  virtual void TearDown() { libaom_test::ClearSystemState(); }
  virtual ~AddNoiseTest() {}
};

double stddev6(char a, char b, char c, char d, char e, char f) {
  const double n = (a + b + c + d + e + f) / 6.0;
  const double v = ((a - n) * (a - n) + (b - n) * (b - n) + (c - n) * (c - n) +
                    (d - n) * (d - n) + (e - n) * (e - n) + (f - n) * (f - n)) /
                   6.0;
  return sqrt(v);
}

TEST_P(AddNoiseTest, CheckNoiseAdded) {
  DECLARE_ALIGNED(16, char, blackclamp[16]);
  DECLARE_ALIGNED(16, char, whiteclamp[16]);
  DECLARE_ALIGNED(16, char, bothclamp[16]);
  const int width = 64;
  const int height = 64;
  const int image_size = width * height;
  char noise[3072];
  const int clamp = aom_setup_noise(4.4, sizeof(noise), noise);

  for (int i = 0; i < 16; i++) {
    blackclamp[i] = clamp;
    whiteclamp[i] = clamp;
    bothclamp[i] = 2 * clamp;
  }

  uint8_t *const s = reinterpret_cast<uint8_t *>(aom_calloc(image_size, 1));
  memset(s, 99, image_size);

  ASM_REGISTER_STATE_CHECK(GetParam()(s, noise, blackclamp, whiteclamp,
                                      bothclamp, width, height, width));

  // Check to make sure we don't end up having either the same or no added
  // noise either vertically or horizontally.
  for (int i = 0; i < image_size - 6 * width - 6; ++i) {
    const double hd = stddev6(s[i] - 99, s[i + 1] - 99, s[i + 2] - 99,
                              s[i + 3] - 99, s[i + 4] - 99, s[i + 5] - 99);
    const double vd = stddev6(s[i] - 99, s[i + width] - 99,
                              s[i + 2 * width] - 99, s[i + 3 * width] - 99,
                              s[i + 4 * width] - 99, s[i + 5 * width] - 99);

    EXPECT_NE(hd, 0);
    EXPECT_NE(vd, 0);
  }

  // Initialize pixels in the image to 255 and check for roll over.
  memset(s, 255, image_size);

  ASM_REGISTER_STATE_CHECK(GetParam()(s, noise, blackclamp, whiteclamp,
                                      bothclamp, width, height, width));

  // Check to make sure don't roll over.
  for (int i = 0; i < image_size; ++i) {
    EXPECT_GT(static_cast<int>(s[i]), clamp) << "i = " << i;
  }

  // Initialize pixels in the image to 0 and check for roll under.
  memset(s, 0, image_size);

  ASM_REGISTER_STATE_CHECK(GetParam()(s, noise, blackclamp, whiteclamp,
                                      bothclamp, width, height, width));

  // Check to make sure don't roll under.
  for (int i = 0; i < image_size; ++i) {
    EXPECT_LT(static_cast<int>(s[i]), 255 - clamp) << "i = " << i;
  }

  aom_free(s);
}

TEST_P(AddNoiseTest, CheckCvsAssembly) {
  DECLARE_ALIGNED(16, char, blackclamp[16]);
  DECLARE_ALIGNED(16, char, whiteclamp[16]);
  DECLARE_ALIGNED(16, char, bothclamp[16]);
  const int width = 64;
  const int height = 64;
  const int image_size = width * height;
  char noise[3072];

  const int clamp = aom_setup_noise(4.4, sizeof(noise), noise);

  for (int i = 0; i < 16; i++) {
    blackclamp[i] = clamp;
    whiteclamp[i] = clamp;
    bothclamp[i] = 2 * clamp;
  }

  uint8_t *const s = reinterpret_cast<uint8_t *>(aom_calloc(image_size, 1));
  uint8_t *const d = reinterpret_cast<uint8_t *>(aom_calloc(image_size, 1));

  memset(s, 99, image_size);
  memset(d, 99, image_size);

  srand(0);
  ASM_REGISTER_STATE_CHECK(GetParam()(s, noise, blackclamp, whiteclamp,
                                      bothclamp, width, height, width));
  srand(0);
  ASM_REGISTER_STATE_CHECK(aom_plane_add_noise_c(
      d, noise, blackclamp, whiteclamp, bothclamp, width, height, width));

  for (int i = 0; i < image_size; ++i) {
    EXPECT_EQ(static_cast<int>(s[i]), static_cast<int>(d[i])) << "i = " << i;
  }

  aom_free(d);
  aom_free(s);
}

INSTANTIATE_TEST_CASE_P(C, AddNoiseTest,
                        ::testing::Values(aom_plane_add_noise_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, AddNoiseTest,
                        ::testing::Values(aom_plane_add_noise_sse2));
#endif

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(MSA, AddNoiseTest,
                        ::testing::Values(aom_plane_add_noise_msa));
#endif
}  // namespace
