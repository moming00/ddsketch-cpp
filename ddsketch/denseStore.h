/*
 * Unless explicitly stated otherwise all files in this repository are licensed
 * under the Apache License 2.0.
 */

#pragma once
#include "pb/ddsketch.pb.h"
#include "constants.h"
#include <deque>
#include <stdexcept>
#include <cmath>
#include <numeric>
#include <sstream>
#include <algorithm>
#include <limits>
#include <memory>

namespace ddsketch {
    static constexpr Index kChunkSize = 128;

    template <typename BinItem>
    class BinList {
    public:
        using Container = std::deque<BinItem>;
        using iterator = typename Container::iterator;
        using const_iterator = typename Container::const_iterator;
        using reference = BinItem&;
        using const_reference = const BinItem&;

        iterator begin() { return data_.begin(); }
        iterator end() { return data_.end(); }
        const_iterator begin() const { return data_.begin(); }
        const_iterator end() const { return data_.end(); }

        BinList() = default;
        ~BinList() = default;
        explicit BinList(size_t size) { initialize_with_zeros(size); }
        BinList(const BinList<BinItem>& bins): data_(bins.data_) { }
        BinList(BinList<BinItem>&& bins) noexcept: data_(std::move(bins.data_)) { }

        BinList& operator=(const BinList<BinItem>& bins) {
            data_ = bins.data_;
            return *this;
        }

        BinList& operator=(BinList<BinItem>&& bins) noexcept {
            data_ = std::move(bins.data_);
            return *this;
        }

        friend std::ostream& operator<<(std::ostream& os, const BinList& bins) {
            for (const auto& elem : bins) {
                os << elem << " ";
            }
            return os;
        }

        size_t size() const { return data_.size(); }
        reference operator[](int idx) { return data_[idx]; }
        const_reference operator[](int idx) const { return data_[idx]; }
        reference first() { return data_[0]; }
        reference last() { return data_[size() - 1]; }
        void insert(BinItem elem) { data_.push_back(elem); }

        BinItem collapsed_count(int start_idx, int end_idx) const {
            if (index_outside_bounds(start_idx) || index_outside_bounds(end_idx)) {
                throw std::invalid_argument("Index out of bounds");
            }
            return std::accumulate(data_.begin() + start_idx, data_.begin() + end_idx, typename decltype(data_)::value_type(0));
        }

        bool has_only_zeros() const {
            auto non_zero_item = std::find_if(data_.begin(), data_.end(), [](const auto& item) { return item != 0; });
            return non_zero_item == data_.end();
        }

        BinItem sum() const {
            return collapsed_count(0, data_.size());
        }

        void initialize_with_zeros(size_t num_zeros) {
            auto trailing_zeros = Container(num_zeros, 0);
            data_ = trailing_zeros;
        }

        void extend_front_with_zeros(size_t count) {
            auto trailing_zeros = Container(count, 0);
            data_.insert(data_.begin(), trailing_zeros.begin(), trailing_zeros.end());
        }

        void extend_back_with_zeros(size_t count) {
            auto trailing_zeros = Container(count, 0);
            data_.insert(data_.end(), trailing_zeros.begin(), trailing_zeros.end());
        }

        void remove_trailing_elements(size_t count) {
            data_.erase(data_.end() - count, data_.end());
        }

        void remove_leading_elements(size_t count) {
            data_.erase(data_.begin(), data_.begin() + count);
        }

        void replace_range_with_zeros(int start_idx, int end_idx, size_t num_zeros) {
            auto zeros = Container(num_zeros, 0);
            data_.erase(data_.begin() + start_idx, data_.begin() + end_idx);
            data_.insert(data_.begin() + start_idx, zeros.begin(), zeros.end());
        }

    private:
        bool index_outside_bounds(size_t idx) const {
            return idx > size();
        }

        Container data_;
    };

    template <typename T>
    struct CRTP {
        T& underlying() {
            return static_cast<T&>(*this);
        }

        T const& underlying() const {
            return static_cast<T const&>(*this);
        }
    };

    // The basic specification of a store
    template <class ConcreteStore>
    class BaseStore: CRTP<ConcreteStore> {
    public:
        // Copy other into this
        void copy(const ConcreteStore& other) {
            this->underlying()->copy(other);
        }

        Index length() {
            return this->underlying()->length();
        }

        bool is_empty() {
            return this->underlying()->is_empty();
        }

        // Updates the counter at the specified index key,
        // growing the number of bins if necessary.
        void add(Index key, RealValue weight) {
            this->underlying()->add(key, weight);
        }

        void add(Index key) {
            this->underlying()->add(key, 1.0);
        }

        // Return the key for the value at given rank
        //  E.g., if the non-zero bins are [1, 1] for keys a, b with no offset
        //
        //  if (lower == true) {
        //       key_at_rank(x) = a for x in [0, 1)
        //       key_at_rank(x) = b for x in [1, 2)
        //  }
        //  if (lower == false) {
        //       key_at_rank(x) = a for x in (-1, 0]
        //       key_at_rank(x) = b for x in (0, 1]
        //  }
        Index key_at_rank(RealValue rank, bool lower = false) const {
            return this->underlying()->key_at_rank(rank, lower);
        }

        // Merge other into this. 
        void merge(const ConcreteStore& other) {
            return this->underlying()->merge(other);
        }

    protected:
        BaseStore() = default;
        ~BaseStore() = default;
        BaseStore(const BaseStore& other) = default;
        BaseStore(BaseStore&& other) noexcept = default;
        BaseStore& operator=(const BaseStore& other) = default;
        BaseStore& operator=(BaseStore&& other) noexcept = default;
    };

    // A dense store that keeps all the bins between the bin for the min_key
    // and the bin for the max_key.
    template <class ConcreteStore = void>
    class BaseDenseStore: public BaseStore<BaseDenseStore<ConcreteStore>> {
    public:
        explicit BaseDenseStore(Index chunk_size = kChunkSize):
            count_(0),
            min_key_(std::numeric_limits<Index>::max()),
            max_key_(std::numeric_limits<Index>::min()),
            chunk_size_(chunk_size),
            offset_(0) {
        }

        std::string to_string() const {
            std::ostringstream repr;
            repr << "{";
            Index i = 0;
            for (const auto& sbin : bins_) {
                repr << i++ + offset_ << ": " << sbin << ", ";
            }
            repr << "}, " << "min_key:" << min_key_ << ", max_key:" << max_key_ << ", offset:" << offset_;

            return repr.str();
        }

        void copy(const BaseDenseStore& other) {
            count_ = other.count_;
            min_key_ = other.min_key_;
            max_key_ = other.max_key_;
            offset_ = other.offset_;
            bins_ = other.bins_;
        }

        const BinList<RealValue>& bins() const { return bins_; }

        Index offset() const { return offset_; }
        RealValue count() const { return count_; }
        Index length() const { return bins_.size(); }

        bool is_empty() const { return length() == kEmptyStoreLength; }

        void add(Index key, RealValue weight = 1.0) {
            bins_[get_index(key)] += weight;
            count_ += weight;
        }

        Index key_at_rank(RealValue rank, bool lower = true) const {
            auto running_ct = 0.0;
            auto idx = 0;
            for (const auto bin_ct : bins_) {
                running_ct += bin_ct;
                if ((lower && running_ct > rank) || (!lower && running_ct >= rank + 1)) {
                    return idx + offset_;
                }
                ++idx;
            }
            return max_key_;
        }

        void merge(const BaseDenseStore& other) {
            if (other.count_ == 0) {
                return;
            }

            if (count_ == 0) {
                copy(other);
                return;
            }

            if (other.min_key_ < min_key_ || other.max_key_ > max_key_) {
                extend_range(other.min_key_, other.max_key_);
            }

            for (auto key = other.min_key_; key <= other.max_key_; ++key) {
                bins_[key - offset_] += other.bins_[key - other.offset_];
            }

            count_ += other.count_;
        }

        ::Store* to_proto() {
            auto result = new ::Store();
            if (is_empty()) {
                return result;
            }
            for (int i = min_key_ - offset_; i <= max_key_ - offset_; i++) {
                result->mutable_contiguousbincounts()->Add(bins_[i]);
            }
            result->set_contiguousbinindexoffset(min_key_);
            return result;
        }

        void merge_with_proto(const ::Store& other) {
            for (auto i : other.bincounts()) {
                add(i.first, i.second);
            }
            auto counts = other.contiguousbincounts();
            auto index = other.contiguousbinindexoffset();
            for (auto count : counts) {
                add(index++, count);
            }
        }

    protected:
        virtual Index get_new_length(Index new_min_key, Index new_max_key) {
            auto desired_length = new_max_key - new_min_key + 1;
            auto num_chunks = std::ceil((1.0 * desired_length) / chunk_size_);

            return chunk_size_ * num_chunks;
        }

        // Adjust the bins, the offset, the min_key, and max_key, without resizing
        // the bins, in order to try making it fit the specified range
        virtual void adjust(Index new_min_key, Index new_max_key) {
            center_bins(new_min_key, new_max_key);

            min_key_ = new_min_key;
            max_key_ = new_max_key;
        }

        // Shift the bins; this changes the offset
        void shift_bins(Index shift) {
            if (shift > 0) {
                bins_.remove_trailing_elements(shift);
                bins_.extend_front_with_zeros(shift);
            } else {
                auto abs_shift = std::abs(shift);
                bins_.remove_leading_elements(abs_shift);
                bins_.extend_back_with_zeros(abs_shift);
            }

            offset_ -= shift;
        }

        // Center the bins; this changes the offset
        void center_bins(Index new_min_key, Index new_max_key) {
            auto middle_key = new_min_key + (new_max_key - new_min_key + 1) / 2;
            shift_bins(offset_ + length() / 2 - middle_key);
        }

        // Grow the bins as necessary and call _adjust
        void extend_range(Index key, Index second_key) {
            auto new_min_key = std::min({ key, second_key, min_key_ });
            auto new_max_key = std::max({ key, second_key, max_key_ });

            if (is_empty()) {
                // Init bins
                auto new_length = get_new_length(new_min_key, new_max_key);
                bins_.initialize_with_zeros(new_length);
                offset_ = new_min_key;
                adjust(new_min_key, new_max_key);
            } else if (new_min_key >= min_key_ &&
                new_max_key < offset_ + length()) {
                // No need to change the range; just update min/max keys
                min_key_ = new_min_key;
                max_key_ = new_max_key;
            } else {
                // Grow the bins
                Index new_length = get_new_length(new_min_key, new_max_key);
                if (new_length > length()) {
                    bins_.extend_back_with_zeros(new_length - length());
                }

                adjust(new_min_key, new_max_key);
            }
        }

        void extend_range(Index key) {
            extend_range(key, key);
        }

        // Calculate the bin index for the key, extending the range if necessary
        virtual Index get_index(Index key) {
            if (key < min_key_ || key > max_key_) {
                extend_range(key);
            }
            return key - offset_;
        }

    public:
        RealValue count_;   // The sum of the counts for the bins
        Index min_key_;     // The minimum key bin
        Index max_key_;     // The maximum key bin
        Index chunk_size_;  // The number of bins to grow by
        Index offset_;      // The difference btw the keys and the index in which they are stored
        BinList<RealValue> bins_;

    private:
        static constexpr size_t kEmptyStoreLength = 0;
    };

    using DenseStore = BaseDenseStore<>;

    // A dense store that keeps all the bins between the bin for the min_key and the
    // bin for the max_key, but collapsing the left-most bins if the number of bins
    // exceeds the bin_limit
    class CollapsingLowestDenseStore: public BaseDenseStore<CollapsingLowestDenseStore> {
    public:
        explicit CollapsingLowestDenseStore(Index bin_limit, Index chunk_size = kChunkSize):
            BaseDenseStore(chunk_size),
            bin_limit_(bin_limit),
            is_collapsed_(false) {
        }

        Index bin_limit() const {
            return bin_limit_;
        }

        void copy(const CollapsingLowestDenseStore& other) {
            count_ = other.count_;
            min_key_ = other.min_key_;
            max_key_ = other.max_key_;
            offset_ = other.offset_;
            bins_ = other.bins_;
            bin_limit_ = other.bin_limit_;
            is_collapsed_ = other.is_collapsed_;
        }

        void merge(const CollapsingLowestDenseStore& other) {
            if (other.count_ == 0) {
                return;
            }

            if (count_ == 0) {
                copy(other);
                return;
            }

            if (other.min_key_ < min_key_ || other.max_key_ > max_key_) {
                extend_range(other.min_key_, other.max_key_);
            }

            auto collapse_start_idx = other.min_key_ - other.offset_;
            auto collapse_end_idx = std::min(min_key_, other.max_key_ + 1) - other.offset_;
            if (collapse_end_idx > collapse_start_idx) {
                auto collapsed_count = bins_.collapsed_count(collapse_start_idx, collapse_end_idx);
                bins_.first() += collapsed_count;
            } else {
                collapse_end_idx = collapse_start_idx;
            }

            for (auto key = collapse_end_idx + other.offset_; key <= other.max_key_; ++key) {
                bins_[key - offset_] += other.bins_[key - other.offset_];
            }

            count_ += other.count_;
        }

    private:
        Index get_new_length(Index new_min_key, Index new_max_key) override {
            auto desired_length = new_max_key - new_min_key + 1;
            Index num_chunks = std::ceil((1.0 * desired_length) / chunk_size_);
            return std::min(chunk_size_ * num_chunks, bin_limit_);
        }

        // Calculate the bin index for the key, extending the range if necessary
        Index get_index(Index key) override {
            if (key < min_key_) {
                if (is_collapsed_) {
                    return 0;
                }
                extend_range(key);

                if (is_collapsed_) {
                    return 0;
                }
            } else if (key > max_key_) {
                extend_range(key);
            }

            return key - offset_;
        }

        // Override. Adjust the bins, the offset, the min_key, and max_key,
        // without resizing the bins, in order to try making it fit the specified
        // range. Collapse to the left if necessary
        void adjust(Index new_min_key, Index new_max_key) override {
            if (new_max_key - new_min_key + 1 > length()) {
                //The range of keys is too wide. the lowest bins need to be collapsed
                new_min_key = new_max_key - length() + 1;

                if (new_min_key >= max_key_) {
                    // Put everything in the first bin
                    offset_ = new_min_key;
                    min_key_ = new_min_key;

                    bins_.initialize_with_zeros(length());
                    bins_.first() = count_;
                } else {
                    auto shift = offset_ - new_min_key;
                    if (shift < 0) {
                        auto collapse_start_index = min_key_ - offset_;
                        auto collapse_end_index = new_min_key - offset_;

                        auto collapsed_count = bins_.collapsed_count(collapse_start_index, collapse_end_index);
                        bins_.replace_range_with_zeros(collapse_start_index, collapse_end_index, new_min_key - min_key_);
                        bins_[collapse_end_index] += collapsed_count;
                        min_key_ = new_min_key;

                        // Shift the buckets to make room for new_max_key
                        shift_bins(shift);
                    } else {
                        min_key_ = new_min_key;

                        // Shift the buckets to make room for new_min_key
                        shift_bins(shift);
                    }
                }

                max_key_ = new_max_key;
                is_collapsed_ = true;
            } else {
                center_bins(new_min_key, new_max_key);

                min_key_ = new_min_key;
                max_key_ = new_max_key;
            }
        }

        Index bin_limit_; // The maximum number of bins
        bool is_collapsed_;
    };

    // A dense store that keeps all the bins between the bin for the min_key and the
    // bin for the max_key, but collapsing the right-most bins if the number of bins
    // exceeds the bin_limit
    class CollapsingHighestDenseStore: public BaseDenseStore<CollapsingHighestDenseStore> {
    public:
        explicit CollapsingHighestDenseStore(Index bin_limit, Index chunk_size = kChunkSize):
            BaseDenseStore(chunk_size),
            bin_limit_(bin_limit),
            is_collapsed_(false) {
        }

        Index bin_limit() const {
            return bin_limit_;
        }

        void copy(const CollapsingHighestDenseStore& other) {
            count_ = other.count_;
            min_key_ = other.min_key_;
            max_key_ = other.max_key_;
            offset_ = other.offset_;
            bins_ = other.bins_;

            bin_limit_ = other.bin_limit_;
            is_collapsed_ = other.is_collapsed_;
        }

        void merge(const CollapsingHighestDenseStore& other) {
            if (other.count_ == 0) {
                return;
            }

            if (count_ == 0) {
                copy(other);
                return;
            }

            if (other.min_key_ < min_key_ || other.max_key_ > max_key_) {
                extend_range(other.min_key_, other.max_key_);
            }

            auto collapse_end_idx = other.max_key_ - other.offset_ + 1;
            auto collapse_start_idx = std::max(max_key_ + 1, other.min_key_) - other.offset_;

            if (collapse_end_idx > collapse_start_idx) {
                auto collapsed_count = bins_.collapsed_count(collapse_start_idx, collapse_end_idx);
                bins_.last() += collapsed_count;
            } else {
                collapse_start_idx = collapse_end_idx;
            }

            for (auto key = other.min_key_; key < collapse_start_idx + other.offset_; ++key) {
                bins_[key - offset_] += other.bins_[key - other.offset_];
            }

            count_ += other.count_;
        }

    private:
        Index get_new_length(Index new_min_key, Index new_max_key) override {
            auto desired_length = new_max_key - new_min_key + 1;
            Index num_chunks = std::ceil((1.0 * desired_length) / chunk_size_);

            return std::min(chunk_size_ * num_chunks, bin_limit_);
        }

        // Calculate the bin index for the key, extending the range if necessary
        Index get_index(Index key) override {
            if (key > max_key_) {
                if (is_collapsed_) {
                    return length() - 1;
                }

                extend_range(key);

                if (is_collapsed_) {
                    return length() - 1;
                }
            } else if (key < min_key_) {
                extend_range(key);
            }

            return key - offset_;
        }

        // Override. Adjust the bins, the offset, the min_key, and max_key, without
        // resizing the bins, in order to try making it fit the specified range.
        // Collapse to the left if necessary.
        void adjust(Index new_min_key, Index new_max_key) override {
            if (new_max_key - new_min_key + 1 > length()) {
                // The range of keys is too wide, the lowest bins need to be collapsed
                new_max_key = new_min_key + length() - 1;
                if (new_max_key <= min_key_) {
                    // Put everything in the last bin
                    offset_ = new_min_key;
                    max_key_ = new_max_key;

                    bins_ = BinList<RealValue>(length());
                    bins_.last() = count_;
                } else {
                    auto shift = offset_ - new_min_key;

                    if (shift > 0) {
                        auto collapse_start_index = new_max_key - offset_ + 1;
                        auto collapse_end_index = max_key_ - offset_ + 1;

                        auto collapsed_count = bins_.collapsed_count(collapse_start_index, collapse_end_index);
                        bins_.replace_range_with_zeros(collapse_start_index, collapse_end_index, max_key_ - new_max_key);
                        bins_[collapse_start_index - 1] += collapsed_count;
                        max_key_ = new_max_key;

                        // Shift the buckets to make room for new_max_key
                        shift_bins(shift);
                    } else {
                        max_key_ = new_max_key;

                        // Shift the buckets to make room for new_min_key
                        shift_bins(shift);
                    }
                }

                min_key_ = new_min_key;
                is_collapsed_ = true;
            } else {
                center_bins(new_min_key, new_max_key);

                min_key_ = new_min_key;
                max_key_ = new_max_key;
            }
        }

        Index bin_limit_; // The maximum number of bins
        bool is_collapsed_;
    };
}