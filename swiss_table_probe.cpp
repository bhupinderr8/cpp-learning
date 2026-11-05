#include <iostream>
#include <arm_neon.h>   // ARM NEON SIMD intrinsics
#include <cstdint>
#include <string>
#include <vector>
#include <bit>

enum CtrlByte : int8_t {
    kEmpty   = static_cast<int8_t>(0x80),
    kDeleted = static_cast<int8_t>(0xFE),
    kSentinel= static_cast<int8_t>(0xFF),
};

struct Entry {
    std::string key;
    uint64_t val;
};

class MinimalSwissGroupARM {
    alignas(16) int8_t ctrl_[16];
    Entry slots_[16];

public:
    MinimalSwissGroupARM() {
        for (int i = 0; i < 16; ++i) {
            ctrl_[i] = kEmpty;
        }
    }

    void insert(size_t local_idx, uint8_t h2, std::string key, uint64_t val) {
        ctrl_[local_idx] = static_cast<int8_t>(h2 & 0x7F);
        slots_[local_idx] = {std::move(key), val};
    }

    const Entry* find(uint8_t h2, const std::string& target_key) const {
        // 1. Broadcast the 7-bit H2 fingerprint across all 16 lanes (vdupq_n_s8)
        int8x16_t target_vec = vdupq_n_s8(static_cast<int8_t>(h2 & 0x7F));

        // 2. Load 16 control bytes from memory in 1 vector load (vld1q_s8)
        int8x16_t ctrl_vec = vld1q_s8(ctrl_);

        // 3. Parallel compare: lane is 0xFF if matched, 0x00 otherwise (vceqq_s8)
        uint8x16_t cmp = vceqq_s8(ctrl_vec, target_vec);

        // 4. Abseil-style NEON bitmask extraction:
        // Pack upper bits of 16 bytes into a 16-nibble / 64-bit mask
        uint16x8_t high_bits = vreinterpretq_u16_u8(cmp);
        // Shift right each 16-bit word and narrow:
        uint8x8_t paired = vshrn_n_u16(high_bits, 4);
        uint64_t compact_mask = vget_lane_u64(vreinterpret_u64_u8(paired), 0);

        // 5. Scan set nibbles (each matched lane occupies 4 bits)
        while (compact_mask != 0) {
            int trailing_zeros = std::countr_zero(compact_mask);
            int local_slot = trailing_zeros / 4; // Each lane contributes 4 bits

            if (slots_[local_slot].key == target_key) {
                return &slots_[local_slot]; // Hit!
            }

            // Clear the 4 bits corresponding to this match
            compact_mask &= ~(0xFULL << (local_slot * 4));
        }

        return nullptr;
    }
};

struct HashParts {
    size_t  h1;
    uint8_t h2;
};

HashParts split_hash(uint64_t hash) {
    return {
        .h1 = static_cast<size_t>(hash >> 7),
        .h2 = static_cast<uint8_t>(hash & 0x7F)
    };
}

int main() {
    MinimalSwissGroupARM group;

    std::string key1 = "storage_node_1";
    uint64_t hash1 = 0xABCD12345678992AULL; // H2 = 0x2A
    HashParts p1 = split_hash(hash1);

    group.insert(5, p1.h2, key1, 9999);

    const Entry* hit = group.find(p1.h2, key1);
    if (hit) {
        std::cout << "[HIT] Found key: " << hit->key << " with val: " << hit->val << "\n";
    }

    std::string key2 = "ghost_node";
    uint64_t hash2 = 0x111122223333443FULL;
    HashParts p2 = split_hash(hash2);

    const Entry* miss = group.find(p2.h2, key2);
    if (!miss) {
        std::cout << "[MISS] Key not found (SIMD mask was 0, zero payload reads)\n";
    }

    return 0;
}