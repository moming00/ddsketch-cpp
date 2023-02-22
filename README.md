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


This project was inspired by https://github.com/tudor1805/sketches-cpp, with additional protobuf feature implemented.

This project was inspired by https://github.com/tudor1805/sketches-cpp, with additional protobuf feature implemented.

Benchamrk test result based on the test script on ./test/main.cpp:
sketch count: 0   merge time: 24µs  serialize sketch: 74µs  deserialize sketch: 105µs   proto size: 15977B
sketch count: 1   merge time: 10µs  serialize sketch: 24µs  deserialize sketch: 64µs    proto size: 16041B
sketch count: 2   merge time: 10µs  serialize sketch: 35µs  deserialize sketch: 67µs    proto size: 16089B
sketch count: 3   merge time: 13µs  serialize sketch: 39µs  deserialize sketch: 111µs   proto size: 16145B
sketch count: 4   merge time: 7µs   serialize sketch: 26µs  deserialize sketch: 65µs    proto size: 16193B
sketch count: 5   merge time: 8µs   serialize sketch: 27µs  deserialize sketch: 63µs    proto size: 16249B
sketch count: 6   merge time: 1µs   serialize sketch: 27µs  deserialize sketch: 64µs    proto size: 16257B
sketch count: 7   merge time: 1µs   serialize sketch: 26µs  deserialize sketch: 63µs    proto size: 16289B
sketch count: 8   merge time: 1µs   serialize sketch: 35µs  deserialize sketch: 84µs    proto size: 16321B
sketch count: 9   merge time: 1µs   serialize sketch: 31µs  deserialize sketch: 65µs    proto size: 16369B
sketch count: 10  merge time: 1µs   serialize sketch: 41µs  deserialize sketch: 77µs    proto size: 16377B
sketch count: 11  merge time: 1µs   serialize sketch: 33µs  deserialize sketch: 66µs    proto size: 16409B
sketch count: 12  merge time: 7µs   serialize sketch: 28µs  deserialize sketch: 66µs    proto size: 16425B
sketch count: 13  merge time: 1µs   serialize sketch: 28µs  deserialize sketch: 69µs    proto size: 16449B
sketch count: 14  merge time: 1µs   serialize sketch: 28µs  deserialize sketch: 67µs    proto size: 16481B
sketch count: 15  merge time: 0µs   serialize sketch: 32µs  deserialize sketch: 67µs    proto size: 16489B
sketch count: 16  merge time: 1µs   serialize sketch: 31µs  deserialize sketch: 65µs    proto size: 16513B
sketch count: 17  merge time: 0µs   serialize sketch: 30µs  deserialize sketch: 65µs    proto size: 16537B
sketch count: 18  merge time: 1µs   serialize sketch: 31µs  deserialize sketch: 65µs    proto size: 16553B
sketch count: 19  merge time: 0µs   serialize sketch: 28µs  deserialize sketch: 64µs    proto size: 16569B
sketch count: 20  merge time: 0µs   serialize sketch: 33µs  deserialize sketch: 65µs    proto size: 16577B
sketch count: 21  merge time: 1µs   serialize sketch: 33µs  deserialize sketch: 86µs    proto size: 16601B
sketch count: 22  merge time: 0µs   serialize sketch: 29µs  deserialize sketch: 65µs    proto size: 16609B
sketch count: 23  merge time: 0µs   serialize sketch: 29µs  deserialize sketch: 63µs    proto size: 16625B
sketch count: 24  merge time: 1µs   serialize sketch: 28µs  deserialize sketch: 64µs    proto size: 16641B
sketch count: 25  merge time: 9µs   serialize sketch: 27µs  deserialize sketch: 61µs    proto size: 16657B
sketch count: 26  merge time: 0µs   serialize sketch: 29µs  deserialize sketch: 65µs    proto size: 16665B
sketch count: 27  merge time: 1µs   serialize sketch: 31µs  deserialize sketch: 65µs    proto size: 16673B
sketch count: 28  merge time: 0µs   serialize sketch: 31µs  deserialize sketch: 65µs    proto size: 16689B
sketch count: 29  merge time: 1µs   serialize sketch: 31µs  deserialize sketch: 68µs    proto size: 16705B
sketch count: 30  merge time: 1µs   serialize sketch: 32µs  deserialize sketch: 67µs    proto size: 16713B
sketch count: 31  merge time: 1µs   serialize sketch: 36µs  deserialize sketch: 71µs    proto size: 16721B
sketch count: 32  merge time: 1µs   serialize sketch: 29µs  deserialize sketch: 66µs    proto size: 16737B
sketch count: 33  merge time: 0µs   serialize sketch: 27µs  deserialize sketch: 64µs    proto size: 16745B
sketch count: 34  merge time: 0µs   serialize sketch: 27µs  deserialize sketch: 64µs    proto size: 16753B
sketch count: 35  merge time: 4µs   serialize sketch: 25µs  deserialize sketch: 62µs    proto size: 16761B
sketch count: 36  merge time: 2µs   serialize sketch: 43µs  deserialize sketch: 118µs   proto size: 16777B
sketch count: 37  merge time: 1µs   serialize sketch: 39µs  deserialize sketch: 106µs   proto size: 16785B