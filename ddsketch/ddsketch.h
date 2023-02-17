/*
 * Unless explicitly stated otherwise all files in this repository are licensed
 * under the Apache License 2.0.
 */

#pragma once

#include "denseStore.h"
#include "indexmapping.h"
#include <limits>
#include "pb/ddsketch.pb.h"

namespace ddsketch {
    template <typename Store, class Mapping>
    class BaseDDSketch {
    public:
        BaseDDSketch(const Mapping& mapping, const Store& store, const Store& negative_store,
            RealValue zerocount = 0.0, RealValue count = 0.0,
            RealValue min = std::numeric_limits<RealValue>::max(),
            RealValue max = std::numeric_limits<RealValue>::min(), RealValue sum = 0.0)
            : mapping_(mapping),
            positive_store_(store),
            negative_store_(negative_store),
            zero_count_(zerocount),
            count_(count),
            min_(min),
            max_(max),
            sum_(sum) {
        }

        static std::string name() { return "DDSketch"; }
        RealValue num_values() const { return count_; }
        RealValue sum() const { return sum_; }
        RealValue avg() const { return sum_ / count_; }

        // Add a value to the sketch with a float64 weight.
        void add(RealValue val, RealValue weight = 1.0) {
            if (weight <= 0.0) {
                throw IllegalArgumentException("Weight must be positive");
            }

            if (val > mapping_.min_indexable()) {
                positive_store_.add(mapping_.key(val), weight);
            } else if (val < -mapping_.min_indexable()) {
                negative_store_.add(mapping_.key(-val), weight);
            } else {
                zero_count_ += weight;
            }

            // Keep track of summary stats
            count_ += weight;
            sum_ += val * weight;

            if (val < min_) {
                min_ = val;
            }

            if (val > max_) {
                max_ = val;
            }
        }

        // Return the value at the specified quantile or NaN if the sketch is empty
        RealValue get_quantile_value(RealValue quantile) {
            auto quantile_value = 0.0;

            if (quantile < 0 || quantile > 1 || count_ == 0) {
                return std::nan("");
            }

            auto rank = quantile * (count_ - 1);
            if (rank < negative_store_.count()) {
                auto reversed_rank = negative_store_.count() - rank - 1;
                auto key = negative_store_.key_at_rank(reversed_rank, false);
                quantile_value = -mapping_.value(key);
            } else if (rank < zero_count_ + negative_store_.count()) {
                return 0.0;
            } else {
                auto key = positive_store_.key_at_rank(rank - zero_count_ - negative_store_.count());
                quantile_value = mapping_.value(key);
            }

            return quantile_value;
        }

        //  Merges the other sketch into this one.
        //  After this operation, this sketch encodes the values that were
        //  added to both this and the input sketch.
        void merge(const BaseDDSketch& other) {
            if (!mergeable(other)) {
                throw UnequalSketchParametersException();
            }

            if (other.count_ == 0) {
                return;
            }

            if (count_ == 0) {
                copy(other);
                return;
            }

            // Merge the stores
            positive_store_.merge(other.positive_store_);
            negative_store_.merge(other.negative_store_);
            zero_count_ += other.zero_count_;

            // Merge summary stats
            count_ += other.count_;
            sum_ += other.sum_;
            min_ = other.min_ < min_ ? other.min_ : min_;
            max_ = other.max_ > max_ ? other.max_ : max_;
        }

        // Two sketches can be merged only if their gammas are equal
        bool mergeable(const BaseDDSketch<Store, Mapping>& other) const {
            return mapping_.gamma() == other.mapping_.gamma();
        }

        // Copy other into this
        void copy(const BaseDDSketch& other) {
            positive_store_.copy(other.positive_store_);
            negative_store_.copy(other.negative_store_);
            zero_count_ = other.zero_count_;
            min_ = other.min_;
            max_ = other.max_;
            count_ = other.count_;
            sum_ = other.sum_;
        }

        ::DDSketch to_proto() {
            ::DDSketch sketch;
            sketch.set_allocated_mapping(mapping_.to_proto());
            sketch.set_allocated_positivevalues(positive_store_.to_proto());
            sketch.set_allocated_negativevalues(negative_store_.to_proto());
            sketch.set_zerocount(zero_count_);
            sketch.set_count(count_);
            sketch.set_min(min_);
            sketch.set_max(max_);
            sketch.set_sum(sum_);
            return sketch;
        }

    protected:
        static Index adjust_bin_limit(Index bin_limit) {
            return bin_limit > 0 ? bin_limit : kDefaultBinLimit;
        }

    private:
        Mapping mapping_;      // Map btw values and store bins
        Store positive_store_; // Storage for positive values
        Store negative_store_; // Storage for negative values
        RealValue zero_count_; // The count of zero values
        RealValue count_;      // The number of values seen by the sketch
        RealValue min_;        // The minimum value seen by the sketch
        RealValue max_;        // The maximum value seen by the sketch
        RealValue sum_;        // The sum of the values seen by the sketch

        static constexpr Index kDefaultBinLimit = 2048;
    };

    //  The default implementation of BaseDDSketch, with optimized memory usage at
    //  the cost of lower ingestion speed, using an unlimited number of bins.
    //  The number of bins will not exceed a reasonable number unless the data is
    //  distributed with tails heavier than any subexponential.
    //  (cf. http://www.vldb.org/pvldb/vol12/p2195-masson.pdf)
    class DDSketch: public BaseDDSketch<DenseStore, LogarithmicMapping> {
    public:
        explicit DDSketch(RealValue relative_accuracy)
            : BaseDDSketch<DenseStore, LogarithmicMapping>(
                LogarithmicMapping(relative_accuracy), DenseStore(), DenseStore()) {
        }
    };

    // Implementation of BaseDDSketch with optimized memory usage at the cost of
    // lower ingestion speed, using a limited number of bins. When the maximum
    // number of bins is reached, bins with lowest indices are collapsed, which
    // causes the relative accuracy to be lost on the lowest quantiles. For the
    // default bin limit, collapsing is unlikely to occur unless the data is
    // distributed with tails heavier than any subexponential.
    // (cf. http://www.vldb.org/pvldb/vol12/p2195-masson.pdf)
    class LogCollapsingLowestDenseDDSketch: public BaseDDSketch<CollapsingLowestDenseStore, LogarithmicMapping> {
    public:
        explicit LogCollapsingLowestDenseDDSketch(RealValue relative_accuracy, Index bin_limit)
            : BaseDDSketch<CollapsingLowestDenseStore, LogarithmicMapping>(
                LogarithmicMapping(relative_accuracy),
                CollapsingLowestDenseStore(adjust_bin_limit(bin_limit)),
                CollapsingLowestDenseStore(adjust_bin_limit(bin_limit))) {
        }
    };

    // Implementation of BaseDDSketch with optimized memory usage at the cost of
    // lower ingestion speed, using a limited number of bins. When the maximum
    // number of bins is reached, bins with highest indices are collapsed, which
    // causes the relative accuracy to be lost on the highest quantiles. For the
    // default bin limit, collapsing is unlikely to occur unless the data is
    // distributed with tails heavier than any subexponential.
    // (cf. http://www.vldb.org/pvldb/vol12/p2195-masson.pdf)
    class LogCollapsingHighestDenseDDSketch: public BaseDDSketch<CollapsingHighestDenseStore, LogarithmicMapping> {
    public:
        LogCollapsingHighestDenseDDSketch(RealValue relative_accuracy, Index bin_limit)
            : BaseDDSketch<CollapsingHighestDenseStore, LogarithmicMapping>(
                LogarithmicMapping(relative_accuracy),
                CollapsingHighestDenseStore(adjust_bin_limit(bin_limit)),
                CollapsingHighestDenseStore(adjust_bin_limit(bin_limit))) {
        }
    };

    // FromProto builds a new instance of DDSketch based on the provided protobuf representation, using a Dense store.
    static BaseDDSketch<DenseStore, IndexMapping> from_proto(::DDSketch* sketch) {
        DenseStore positive_values, negative_values;
        positive_values.merge_with_proto(sketch->positivevalues());
        negative_values.merge_with_proto(sketch->negativevalues());

        auto m = sketch->mapping();
        switch (m.interpolation()) {
        case ::IndexMapping_Interpolation_NONE:
            return BaseDDSketch<DenseStore, IndexMapping>(
                LogarithmicMapping(m.alpha(), m.indexoffset()), positive_values, negative_values, sketch->zerocount(),
                sketch->count(), sketch->min(), sketch->max(), sketch->sum());
        case ::IndexMapping_Interpolation_LINEAR:
            return BaseDDSketch<DenseStore, IndexMapping>(
                LinearlyInterpolatedMapping(m.alpha(), m.indexoffset()), positive_values, negative_values, sketch->zerocount(),
                sketch->count(), sketch->min(), sketch->max(), sketch->sum());
        case ::IndexMapping_Interpolation_CUBIC:
            return BaseDDSketch<DenseStore, IndexMapping>(
                CubicallyInterpolatedMapping(m.alpha(), m.indexoffset()), positive_values, negative_values, sketch->zerocount(),
                sketch->count(), sketch->min(), sketch->max(), sketch->sum());
        default:
            throw IllegalArgumentException("interpolation not supported: " + std::to_string(m.interpolation()));
        }
    }

} // namespace ddsketch
