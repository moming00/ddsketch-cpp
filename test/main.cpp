/*
 * Unless explicitly stated otherwise all files in this repository are licensed
 * under the Apache License 2.0.
 */

#include "../ddsketch/ddsketch.h"
#include <iostream>
#include <random>
#include <chrono>

constexpr auto kDesiredRelativeAccuracy = 0.01;
int main() {
    ddsketch::DDSketch sketch(kDesiredRelativeAccuracy);

    std::default_random_engine generator(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::normal_distribution<double> distribution(0.0, 1.0);

    // auto now = std::chrono::high_resolution_clock::now();
    for (auto value = 1; value <= 50000000; ++value) {
        sketch.add(distribution(generator));
    }
    // auto now1 = std::chrono::high_resolution_clock::now();
    // std::cout << "Time took to generate data: "
    //     << std::chrono::duration_cast<std::chrono::seconds>(now1 - now).count() << "s\n";

    // const auto quantiles = {
    //     0.01, 0.05, 0.10, 0.20, 0.25,
    //     0.40, 0.50, 0.60, 0.75, 0.85,
    //     0.95, 0.96, 0.97, 0.98, 0.99 };

    // std::cout.precision(std::numeric_limits<double>::max_digits10);
    // for (const auto quantile : quantiles) {
    //     std::cout << "Quantile: " << int(quantile * 100) << "%: "
    //         << sketch.get_quantile_value(quantile) << "\n";
    // }
    // std::cout << "Time took to caculate quantile: "
    //     << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - now1).count() << "µs\n";

    std::chrono::microseconds totalDuration;
    for (int i = 0; i < 10000; i++) {
        ddsketch::DDSketch anotherSketch(kDesiredRelativeAccuracy);
        for (auto value = 1; value <= 50000000; ++value) {
            anotherSketch.add(distribution(generator) + i);
        }

        auto now = std::chrono::high_resolution_clock::now();
        sketch.merge(anotherSketch);

        auto now1 = std::chrono::high_resolution_clock::now();
        auto psketch = sketch.to_proto();
        auto output = psketch.SerializeAsString();

        auto now2 = std::chrono::high_resolution_clock::now();
        ::DDSketch tsketch;
        tsketch.ParseFromString(output);
        auto s = ddsketch::from_proto(&tsketch);
        auto now3 = std::chrono::high_resolution_clock::now();

        totalDuration += std::chrono::duration_cast<std::chrono::microseconds>(now3 - now);
        std::cout
            << "    sketch count: " << i
            << "    merge time: " << std::chrono::duration_cast<std::chrono::microseconds>(now1 - now).count() << "µs"
            << "    serialize sketch: " << std::chrono::duration_cast<std::chrono::microseconds>(now2 - now1).count() << "µs"
            << "    deserialize sketch: " << std::chrono::duration_cast<std::chrono::microseconds>(now3 - now2).count() << "µs"
            << "    proto size: " << output.length() << "B" << std::endl;
    }
    std::cout << "Total time took to merge sketch: " << totalDuration.count() << "µs\n";
}