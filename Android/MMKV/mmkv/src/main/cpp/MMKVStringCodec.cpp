/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MMKVStringCodec.h"

namespace mmkv::android {
namespace {

inline bool isContinuation(uint8_t value) {
    return value >= 0x80 && value <= 0xBF;
}

bool decodeCanonicalUtf8(std::string_view input, std::vector<uint16_t> &output) {
    output.clear();
    output.reserve(input.size());

    size_t index = 0;
    while (index < input.size()) {
        const auto first = static_cast<uint8_t>(input[index]);
        if (first <= 0x7F) {
            output.push_back(first);
            index++;
            continue;
        }

        if (first >= 0xC2 && first <= 0xDF) {
            if (input.size() - index < 2) {
                return false;
            }
            const auto second = static_cast<uint8_t>(input[index + 1]);
            if (!isContinuation(second)) {
                return false;
            }
            output.push_back(static_cast<uint16_t>(((first & 0x1F) << 6) | (second & 0x3F)));
            index += 2;
            continue;
        }

        if (first >= 0xE0 && first <= 0xEF) {
            if (input.size() - index < 3) {
                return false;
            }
            const auto second = static_cast<uint8_t>(input[index + 1]);
            const auto third = static_cast<uint8_t>(input[index + 2]);
            if (!isContinuation(second) || !isContinuation(third)) {
                return false;
            }
            // Reject overlong encodings and UTF-16 surrogate code points.
            if ((first == 0xE0 && second < 0xA0) || (first == 0xED && second > 0x9F)) {
                return false;
            }
            output.push_back(static_cast<uint16_t>(((first & 0x0F) << 12) |
                                                   ((second & 0x3F) << 6) |
                                                   (third & 0x3F)));
            index += 3;
            continue;
        }

        if (first >= 0xF0 && first <= 0xF4) {
            if (input.size() - index < 4) {
                return false;
            }
            const auto second = static_cast<uint8_t>(input[index + 1]);
            const auto third = static_cast<uint8_t>(input[index + 2]);
            const auto fourth = static_cast<uint8_t>(input[index + 3]);
            if (!isContinuation(second) || !isContinuation(third) || !isContinuation(fourth)) {
                return false;
            }
            // Keep the scalar value within U+10000...U+10FFFF.
            if ((first == 0xF0 && second < 0x90) || (first == 0xF4 && second > 0x8F)) {
                return false;
            }
            const uint32_t scalar = ((first & 0x07) << 18) |
                                    ((second & 0x3F) << 12) |
                                    ((third & 0x3F) << 6) |
                                    (fourth & 0x3F);
            const uint32_t supplementary = scalar - 0x10000;
            output.push_back(static_cast<uint16_t>(0xD800 + (supplementary >> 10)));
            output.push_back(static_cast<uint16_t>(0xDC00 + (supplementary & 0x3FF)));
            index += 4;
            continue;
        }

        return false;
    }
    return true;
}

bool decodeModifiedUtf8(std::string_view input, std::vector<uint16_t> &output) {
    output.clear();
    output.reserve(input.size());

    size_t index = 0;
    while (index < input.size()) {
        const auto first = static_cast<uint8_t>(input[index]);
        if (first >= 0x01 && first <= 0x7F) {
            output.push_back(first);
            index++;
            continue;
        }

        if (first == 0xC0) {
            if (input.size() - index < 2 || static_cast<uint8_t>(input[index + 1]) != 0x80) {
                return false;
            }
            output.push_back(0);
            index += 2;
            continue;
        }

        if (first >= 0xC2 && first <= 0xDF) {
            if (input.size() - index < 2) {
                return false;
            }
            const auto second = static_cast<uint8_t>(input[index + 1]);
            if (!isContinuation(second)) {
                return false;
            }
            output.push_back(static_cast<uint16_t>(((first & 0x1F) << 6) | (second & 0x3F)));
            index += 2;
            continue;
        }

        if (first >= 0xE0 && first <= 0xEF) {
            if (input.size() - index < 3) {
                return false;
            }
            const auto second = static_cast<uint8_t>(input[index + 1]);
            const auto third = static_cast<uint8_t>(input[index + 2]);
            if (!isContinuation(second) || !isContinuation(third) || (first == 0xE0 && second < 0xA0)) {
                return false;
            }
            // Modified UTF-8 encodes UTF-16 code units, so surrogate code units
            // are intentionally accepted here (including unpaired surrogates).
            output.push_back(static_cast<uint16_t>(((first & 0x0F) << 12) |
                                                   ((second & 0x3F) << 6) |
                                                   (third & 0x3F)));
            index += 3;
            continue;
        }

        // Raw NUL, C1, continuation bytes, and four-byte UTF-8 are not MUTF-8.
        return false;
    }
    return true;
}

} // namespace

bool decodeUtf8OrModifiedUtf8(std::string_view input, std::vector<uint16_t> &output) {
    std::vector<uint16_t> decoded;
    if (decodeCanonicalUtf8(input, decoded) || decodeModifiedUtf8(input, decoded)) {
        output.swap(decoded);
        return true;
    }
    output.clear();
    return false;
}

} // namespace mmkv::android
