# ddsketch-cpp
A cpp implementation of DDSketch

DDSketch, a quantile sketch with relative-error guarantees. This sketch computes quantile values with an approximation error that is relative to the actual quantile value. It works on both negative and non-negative input values.

For instance, using DDSketch with a relative accuracy guarantee set to 1%, if the expected quantile value is 100, the computed quantile value is guaranteed to be between 99 and 101. If the expected quantile value is 1000, the computed
quantile value is guaranteed to be between 990 and 1010.
DDSketch works by mapping floating-point input values to bins and counting the number of values for each bin. The underlying structure that keeps track of bin counts is store.

The memory size of the sketch depends on the range that is covered by the input values: the larger that range, the more bins are needed to keep track of the input values. As a rough estimate, if working on durations with a relative accuracy of 2%, about 2kB (275 bins) are needed to cover values between 1 millisecond and 1 minute, and about 6kB (802 bins) to cover values between 1 nanosecond and 1 day.

The size of the sketch can be have a fail-safe upper-bound by using collapsing stores. As shown in http://www.vldb.org/pvldb/vol12/p2195-masson.pdf the likelihood of a store collapsing when using the default bound is vanishingly small for most data.

DDSketch implementations are also available in:
    https://github.com/DataDog/sketches-go/
    https://github.com/DataDog/sketches-py/
    https://github.com/DataDog/sketches-js/


This project was inspired by https://github.com/tudor1805/sketches-cpp, 
with additional protobuf feature implemented.