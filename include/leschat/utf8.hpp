#pragma once

#include <cstdint>
#include <string_view>

namespace leschat {

inline bool is_valid_utf8(std::string_view text) {
    std::size_t index = 0;

    while (index < text.size()) {
        const auto first =
            static_cast<unsigned char>(text[index]);

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;

        if (first <= 0x7F) {
            continuation_count = 0;
            code_point = first;
        } else if (
            first >= 0xC2 &&
            first <= 0xDF
        ) {
            continuation_count = 1;
            code_point = first & 0x1F;
        } else if (
            first >= 0xE0 &&
            first <= 0xEF
        ) {
            continuation_count = 2;
            code_point = first & 0x0F;
        } else if (
            first >= 0xF0 &&
            first <= 0xF4
        ) {
            continuation_count = 3;
            code_point = first & 0x07;
        } else {
            return false;
        }

        if (index + continuation_count >= text.size()) {
            return false;
        }

        for (std::size_t offset = 1;
             offset <= continuation_count;
             ++offset) {
            const auto next = static_cast<unsigned char>(
                text[index + offset]
            );

            if ((next & 0xC0) != 0x80) {
                return false;
            }

            code_point =
                (code_point << 6) |
                (next & 0x3F);
        }

        if (
            (continuation_count == 2 &&
             code_point < 0x800) ||
            (continuation_count == 3 &&
             code_point < 0x10000) ||
            code_point > 0x10FFFF ||
            (code_point >= 0xD800 &&
             code_point <= 0xDFFF)
        ) {
            return false;
        }

        index += continuation_count + 1;
    }

    return true;
}

}  // namespace leschat
