/*
 * Unless explicitly stated otherwise all files in this repository are licensed
 * under the Apache License 2.0.
 */

#pragma once

#include <exception>
#include <string>

namespace ddsketch {

    using RealValue = double;
    using Index = int64_t;

    // Exception when facing unexpected arguments
    class IllegalArgumentException: public std::exception {
    public:
        const char* what() const noexcept override { return message_.c_str(); }
        explicit IllegalArgumentException(const std::string& message): message_(message) { }

    private:
        std::string message_;
    };

    // Exception when trying to merge two sketches with different relative_accuracy parameters
    class UnequalSketchParametersException: public std::exception {
    public:
        const char* what() const noexcept override {
            return "Cannot merge two DDSketches with different parameters";
        }
    };
}  // namespace ddsketch
