#ifndef BITVECTOR_BITVECTOR_H
#define BITVECTOR_BITVECTOR_H

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>

#define EVERY_OTHER_1_POS 8192          // Save position of every ~8 thousandth One. Select-cache distance
#define BLOCK_SIZE 512                  // Block size in bit.
#define L0BLOCK_SIZE 0xFFFFFFFFFFF      // 2^45 - 1, so 44 1s

// uint64_t is implementation defined long or long long, which shouldn't be the case
// but for some reason it can be.
typedef unsigned long long uint64;

/**
 * The bitvector class, defining all public and private methods.
 */
class bitvector {

    // Javadoc-style comments can be found on every method implementation.

public:
    explicit bitvector(std::string& str);
    uint16_t access(uint64& ptr);
    uint64 rank(uint64 ptr, uint8_t& bitValue);
    uint64 select(uint64& ptr, uint8_t byteValue);
    void buildHelpers();
    uint64 size();
private:
    uint64 rank_1(uint64& ptr);
    uint64 superRank(uint64& ptr);
    uint64 select_0(uint64& num);
    uint64 select_1(uint64& num);
    uint64 select_0_iterative(uint64& num, uint64 start, uint64 end);
    uint64 select_1_iterative(uint64& num, uint64 start, uint64 end);

    // First, some overhead variables to store metadata about the bitvector.
    // Secondly, the vector and three helper structures.

    uint64 L0SingleBlockData;
    uint64 oneCount, zeroCount, lastOnePos, lastZeroPos;
    std::vector<uint64> vector;
    std::vector<uint64> superBlocks;
    std::vector<uint32_t> selectCache_0;
    std::vector<uint32_t> selectCache_1;
};

#endif