/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdlib>
#include <vector>

#include "av1/encoder/cost.h"
#include "av1/encoder/tpl_model.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

double laplace_prob(double q_step, double b, double zero_bin_ratio,
                    int qcoeff) {
  int abs_qcoeff = abs(qcoeff);
  double z0 = fmax(exp(-zero_bin_ratio / 2 * q_step / b), TPL_EPSILON);
  if (abs_qcoeff == 0) {
    double p0 = 1 - z0;
    return p0;
  } else {
    assert(abs_qcoeff > 0);
    double z = fmax(exp(-q_step / b), TPL_EPSILON);
    double p = z0 / 2 * (1 - z) * pow(z, abs_qcoeff - 1);
    return p;
  }
}

TEST(TplModelTest, TransformCoeffEntropyTest1) {
  // Check the consistency between av1_estimate_coeff_entropy() and
  // laplace_prob()
  double b = 1;
  double q_step = 1;
  double zero_bin_ratio = 2;
  for (int qcoeff = -256; qcoeff < 256; ++qcoeff) {
    double rate = av1_estimate_coeff_entropy(q_step, b, zero_bin_ratio, qcoeff);
    double prob = laplace_prob(q_step, b, zero_bin_ratio, qcoeff);
    double ref_rate = -log2(prob);
    EXPECT_DOUBLE_EQ(rate, ref_rate);
  }
}

TEST(TplModelTest, TransformCoeffEntropyTest2) {
  // Check the consistency between av1_estimate_coeff_entropy(), laplace_prob()
  // and av1_laplace_entropy()
  double b = 1;
  double q_step = 1;
  double zero_bin_ratio = 2;
  double est_expected_rate = 0;
  for (int qcoeff = -20; qcoeff < 20; ++qcoeff) {
    double rate = av1_estimate_coeff_entropy(q_step, b, zero_bin_ratio, qcoeff);
    double prob = laplace_prob(q_step, b, zero_bin_ratio, qcoeff);
    est_expected_rate += prob * rate;
  }
  double expected_rate = av1_laplace_entropy(q_step, b, zero_bin_ratio);
  EXPECT_NEAR(expected_rate, est_expected_rate, 0.001);
}

TEST(TplModelTest, DeltaRateCostZeroFlow) {
  // When srcrf_dist equal to recrf_dist, av1_delta_rate_cost should return 0
  int64_t srcrf_dist = 256;
  int64_t recrf_dist = 256;
  int64_t delta_rate = 512;
  int pixel_num = 256;
  int64_t rate_cost =
      av1_delta_rate_cost(delta_rate, recrf_dist, srcrf_dist, pixel_num);
  EXPECT_EQ(rate_cost, 0);
}

// a reference function of av1_delta_rate_cost() with delta_rate using bit as
// basic unit
double ref_delta_rate_cost(double delta_rate, double src_rec_ratio,
                           int pixel_count) {
  assert(src_rec_ratio <= 1 && src_rec_ratio >= 0);
  double bits_per_pixel = delta_rate / pixel_count;
  double p = pow(2, bits_per_pixel);
  double flow_rate_per_pixel =
      sqrt(p * p / (src_rec_ratio * p * p + (1 - src_rec_ratio)));
  double rate_cost = pixel_count * log2(flow_rate_per_pixel);
  return rate_cost;
}

TEST(TplModelTest, DeltaRateCostReference) {
  int64_t scale = 1 << (TPL_DEP_COST_SCALE_LOG2 + AV1_PROB_COST_SHIFT);
  std::vector<int64_t> srcrf_dist_arr = { 256, 257, 312 };
  std::vector<int64_t> recrf_dist_arr = { 512, 288, 620 };
  std::vector<int64_t> delta_rate_arr = { 10, 278, 100 };
  for (size_t t = 0; t < srcrf_dist_arr.size(); ++t) {
    int64_t srcrf_dist = srcrf_dist_arr[t];
    int64_t recrf_dist = recrf_dist_arr[t];
    int64_t delta_rate = delta_rate_arr[t];
    int64_t scaled_delta_rate = delta_rate * scale;
    int pixel_count = 256;
    double rate_cost = av1_delta_rate_cost(scaled_delta_rate, recrf_dist,
                                           srcrf_dist, pixel_count);
    rate_cost /= scale;
    double src_rec_ratio = (double)srcrf_dist / recrf_dist;
    double ref_rate_cost =
        ref_delta_rate_cost(delta_rate, src_rec_ratio, pixel_count);
    EXPECT_NEAR(rate_cost, ref_rate_cost, 1);
  }
}

}  // namespace
