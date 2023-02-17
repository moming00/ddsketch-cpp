/*
 * Unless explicitly stated otherwise all files in this repository are licensed
 * under the Apache License 2.0.
 */

#pragma once
#include "constants.h"
#include <cmath>
#include <limits>
#include "pb/ddsketch.pb.h"

// A mapping between values and integer indices that imposes relative accuracy
// guarantees. Specifically, for any value `minIndexableValue() < value <
// maxIndexableValue` implementations of `IndexMapping` must be such that
// `value(key(v))` is close to `v` with a relative error that is less than
// `relative_accuracy`.
//
// In implementations of IndexMapping, there is generally a trade-off between the
// cost of computing the key and the number of keys that are required to cover a
// given range of values (memory optimality). The most memory-optimal mapping is
// the LogarithmicMapping, but it requires the costly evaluation of the logarithm
// when computing the index. Other mappings can approximate the logarithmic mapping,
// while being less computationally costly.
namespace ddsketch {
    class IndexMapping {
    public:
        // Return the mapped bucket index
        Index key(RealValue value) {
            return static_cast<Index>(std::ceil(log_gamma(value)) + indexOffset);
        }

        // Return the value based on a buckect index
        RealValue value(Index key) { return pow_gamma(key - indexOffset) * (2.0 / (1 + gamma_)); }

        virtual ::IndexMapping::Interpolation interpolation() { return IndexMapping_Interpolation_NONE; }

        RealValue gamma() const { return gamma_; }
        RealValue min_indexable() const { return minIndexableValue; }
        RealValue max_indexable() const { return maxIndexableValue; }
        RealValue multiplier() const { return multiplier_; }
        RealValue& multiplier() { return multiplier_; }

        ::IndexMapping* to_proto() {
            auto result = new ::IndexMapping();
            result->set_alpha(alpha_);
            result->set_indexoffset(indexOffset);
            result->set_interpolation(interpolation());
            return result;
        }

    protected:
        explicit IndexMapping(RealValue relative_accuracy, RealValue offset = 0.0) {
            if (relative_accuracy <= 0.0 || relative_accuracy >= 1.0) {
                throw IllegalArgumentException("Relative accuracy must be between 0 and 1");
            }

            indexOffset = offset;
            alpha_ = relative_accuracy;
            gamma_ = (1 + relative_accuracy) / (1 - relative_accuracy);
            multiplier_ = 1.0 / std::log(gamma_);
            minIndexableValue = std::numeric_limits<RealValue>::min() * gamma_;
            maxIndexableValue = std::numeric_limits<RealValue>::max() / gamma_;
        }

        // Return (an approximation of) the logarithm of the value base gamma
        virtual RealValue log_gamma(RealValue value) { return 0; }

        // Return (an approximation of) gamma to the power value
        virtual RealValue pow_gamma(RealValue value) { return 0; }

    private:
        RealValue indexOffset;       // Offset to shift all bin keys
        RealValue alpha_;            // relative_accuracy, keep for proto usage only
        RealValue gamma_;            // gamma = (1 + alpha) / (1 - alpha)
        RealValue minIndexableValue; // The smallest value the sketch can distinguish from 0
        RealValue maxIndexableValue; // The largest value the sketch can handle
        RealValue multiplier_;       // to simiplify the math, multiplier = 1 / log(gamma)
    };

    // A memory-optimal IndexMapping, i.e, given a targeted relative accuracy,
    // it requires the least number of keys to cover a given range of values.
    // This is done by logarithmically mapping floating-point values to integers.
    class LogarithmicMapping: public IndexMapping {
    public:
        explicit LogarithmicMapping(RealValue relative_accuracy, RealValue offset = 0.0)
            : IndexMapping(relative_accuracy, offset) {
            multiplier() *= std::log(2.0);
        }

        ::IndexMapping::Interpolation interpolation() {
            return ::IndexMapping_Interpolation_NONE;
        }

    private:
        RealValue log_gamma(RealValue value) override {
            return std::log2(value) * multiplier();
        }

        RealValue pow_gamma(RealValue value) override {
            return std::exp2(value / multiplier());
        }
    };

    // LinearlyInterpolatedMapping is a fast IndexMapping that approximates the
    // memory-optimal LogarithmicMapping by extracting the floor value of the
    // logarithm to the base 2 from the binary representations of floating-point
    // values and linearly interpolating the logarithm in-between.
    class LinearlyInterpolatedMapping: public IndexMapping {
    public:
        explicit LinearlyInterpolatedMapping(RealValue relative_accuracy, RealValue offset = 0.0)
            : IndexMapping(relative_accuracy, offset) {
        }

        ::IndexMapping::Interpolation interpolation() {
            return ::IndexMapping_Interpolation_LINEAR;
        }

    private:
        // Approximates log2 by s + f
        // where v = (s+1) * 2 ** f  for s in [0, 1)
        // frexp(v) returns m and e s.t.
        // v = m * 2 ** e ; (m in [0.5, 1) or 0.0)
        // so we adjust m and e accordingly
        static RealValue log2_approx(RealValue value) {
            auto exponent = 0;
            auto mantissa = std::frexp(value, &exponent);
            auto significand = 2.0 * mantissa - 1;
            return significand + (exponent - 1);
        }

        // Inverse of log2_approx
        static RealValue exp2_approx(RealValue value) {
            auto exponent = std::floor(value) + 1;
            auto mantissa = (value - exponent + 2) / 2.0;
            return std::ldexp(mantissa, exponent);
        }

        RealValue log_gamma(RealValue value) override {
            return log2_approx(value) * multiplier();
        }

        RealValue pow_gamma(RealValue value) override {
            return exp2_approx(value / multiplier());
        }
    };

    // CubicallyInterpolatedMapping is a fast IndexMapping that approximates the
    // memory-optimal LogarithmicMapping by extracting the floor value of the
    // logarithm to the base 2 from the binary representations of floating-point
    // values and cubically interpolating the logarithm in-between. More detailed
    // documentation of this method can be found in: <a
    // href="https://github.com/DataDog/sketches-java/">sketches-java</a>
    class CubicallyInterpolatedMapping: public IndexMapping {
    public:
        explicit CubicallyInterpolatedMapping(RealValue relative_accuracy, RealValue offset = 0.0)
            : IndexMapping(relative_accuracy, offset) {
            multiplier() /= C_;
        }

        ::IndexMapping::Interpolation interpolation() {
            return ::IndexMapping_Interpolation_CUBIC;
        }

    private:
        // Approximates log2 using a cubic polynomial
        static RealValue cubic_log2_approx(RealValue value) {
            auto exponent = 0;
            auto mantissa = std::frexp(value, &exponent);
            auto significand = 2.0 * mantissa - 1;

            return ((A_ * significand + B_) * significand + C_) * significand + (exponent - 1);
        }

        // Derived from Cardano's formula
        static RealValue cubic_exp2_approx(RealValue value) {
            auto floor_value = std::floor(value);
            auto exponent = static_cast<int>(floor_value);
            auto delta_0 = B_ * B_ - 3 * A_ * C_;
            auto delta_1 = (2 * B_ * B_ * B_ - 9 * A_ * B_ * C_ - 27 * A_ * A_ * (value - exponent));
            auto cardano = std::cbrt((delta_1 - std::sqrt(delta_1 * delta_1 - 4 * delta_0 * delta_0 * delta_0)) / 2.0);
            auto significand_plus_one = -(B_ + cardano + delta_0 / cardano) / (3 * A_) + 1;
            auto mantissa = significand_plus_one / 2.0;

            return std::ldexp(mantissa, exponent + 1);
        }

        RealValue log_gamma(RealValue value) override {
            return cubic_log2_approx(value) * multiplier();
        }

        RealValue pow_gamma(RealValue value) override {
            return cubic_exp2_approx(value / multiplier());
        }

        static constexpr RealValue A_ = 6.0 / 35;
        static constexpr RealValue B_ = -3.0 / 5;
        static constexpr RealValue C_ = 10.0 / 7;
    };
}