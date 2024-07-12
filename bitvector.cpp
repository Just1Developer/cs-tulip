#include "bitvector.h"

#include <bit>

/**
 * Creates a new bitvector. Initializes all values to prevent unwanted errors and warnings,
 * and reads the bitvector from a string to a vector\<uint64>.<br/>
 * To reduce shift operations, the bits inside a 64-bit word are stored from right to left, so
 * in reverse order.<br/>
 * This string may end with window's line break remnant of \\r.
 * @param str The string.
 */
bitvector::bitvector(std::string& str) {
    // Initialize Values
    L0SingleBlockData = 0;
    zeroCount = 0;
    oneCount = 0;
    lastOnePos = 0;
    lastZeroPos = 0;

    // div by 64 + 1 for rounding
    vector = std::vector<uint64>((str.length() >> 6) + 1);
    char innerIndex = 0;
    uint64 outerIndex = 0;
    uint64 currentConstruction = 0;
    for (const char& c : str) {
        if (c > '1' || c < '0') continue;  // Because of windows \r\n line break stuff, this condition is only false for '0' and '1'

        currentConstruction |= ((uint64) (c - '0')) << innerIndex;

        ++innerIndex;
        if (innerIndex >= 64) {
            innerIndex = 0;
            vector[outerIndex++] = currentConstruction;
            currentConstruction = 0;
        }
    }
    // Add last edited too
    vector[outerIndex] = currentConstruction;
}

/**
 * Returns the bit value at the given position. The position is 0-based,
 * meaning the first bit has the position 0.
 * @param ptr The position.
 * @return The bit value at that position.
 */
uint16_t bitvector::access(uint64& ptr) {
    // 64 bit per entry, stored backwards
    // Get 64 bit segment in the vector, then shift by ptr % 64, and get the resulting first bit with &1
    return (vector[ptr >> 6] >> (ptr & ((1 << 6) - 1))) & 1;
}

/**
 * Gets the number of bit that had the given bit value, 0 or 1, before the provided
 * position. rank(0, any) is always 0 because before position 0 there is no zero or one, ever.<br/>
 * If the position is too large, it is reduced to the maximum allowed value.
 * @param ptr The position.
 * @param bitValue The value of the bit, 0 or 1.
 * @return The amount of bits with the value 0 or 1 provided in bitValue that occurred before ptr.
 */
uint64 bitvector::rank(uint64 ptr, uint8_t& bitValue) {
    if (ptr <= 0) return 0;
    if (ptr > (vector.size() << 6) - 1) ptr = (vector.size() << 6) - 1;
    if (bitValue == 1) return rank_1(ptr);
    else return ptr - rank_1(ptr);    // Number of total bits minus number of 1s. This works because bits are either 0 or 1.
}

/**
 * An internal method. This calculates the rank ones before a given position.<br/>
 * The rank(...) method uses this in both cases. When trying to find how many zeros occurred before X,
 * since bits can only be 0 or 1, we can see how many bits there were before X in general (since positions are
 * 0-based, it's exactly X), and simply subtract the amount of 1s. What remains must logically be the amount of zeros.<br/>
 * <br/>
 * Using some clever parsing of the super block index and block index from the provided position, we can extract the amount
 * of 1s before the relevant 512-bit block from the superblock's metadata. We then iterate over how many 64-bit words inside
 * the 512-bit block come before the one the position is in, count the 1s using popcount and add it to the sum.<br/>
 * Then, we overlay a mask (1 << X)-1 on the relevant 64-bit word to cover only the bits 'before' the position with, scrapping
 * all 1s we do not want to count. Then, we simply count the remaining ones. Adding everything together results in the proper rank.
 * @param ptr The position.
 * @return The amount of 1s before the position.
 */
uint64 bitvector::rank_1(uint64& ptr) {
    // Cut the 13 bit but then multiply by 2. Like this to erase the last bit
    uint64 superblockIndex = (ptr >> 12) << 1;
    uint64 metadata1 = superBlocks[superblockIndex];
    uint64 metadata2 = superBlocks[superblockIndex + 1];
    // Get identifying bits for the 512-segment, which are 2^10 to 2^13 (excl.)
    auto blockId = (uint8_t) ((ptr >> 9) & 0b111);
    // Now, get metadata from overhead
    uint64 preOnesCount = (metadata1 >> 20) + (ptr > L0BLOCK_SIZE ? L0SingleBlockData : 0);

    if (blockId == 1) {
        preOnesCount += ((metadata1 >> 8) & 0xFFF);
    } else if (blockId == 2) {
        preOnesCount += ((metadata1 & 0xFF) << 4) | (metadata2 >> 60);
    } else if (blockId > 2) {
        preOnesCount += ((metadata2 >> ((blockId - 3) * 12)) & 0xFFF);
    }

    uint16_t blockSum = 0;
    // Begin index in the vector of the 512 block. Important to clear the last 3 bit to 0, hence the shifts
    uint64 beginBlockIndex = (ptr >> 9) << 3;
    uint8_t which64BitWord = (ptr >> 6) & 0x7;

    // 64-bit words in this 512-bit block before the segment our pos is in
    for (uint8_t k = 0; k < which64BitWord; ++k) {
        blockSum += std::popcount(vector[beginBlockIndex + k]);
    }

    uint64 wordCoverageMask = (1ULL << (ptr & 0x3F)) - 1;    // Get a mask for the 64-bit word
    blockSum += std::popcount(vector[beginBlockIndex + which64BitWord] & wordCoverageMask);

    return preOnesCount + blockSum;
}

/**
 * Gets the rank from the metadata of the superblock that ptr is in.
 * This will not get the accurate rank, but rather a minimum rank. Used when the exact rank isn't needed,
 * but just an estimate, for example in the select search.
 * @param ptr The absolut index.
 * @return The rank of the super block the ptr is in.
 */
uint64 bitvector::superRank(uint64& ptr) {
    uint64 metadata1 = superBlocks[ptr << 1];
    return ((ptr > L0BLOCK_SIZE) ? L0SingleBlockData : 0) + (metadata1 >> 20);
}

/**
 * Select returns the position of the ith 1 or 0. For obvious reasons, select(0, byteValue) returns 0.
 * @param i The ith 1 or 0 to get the position for.
 * @param byteValue 1 or 0.
 * @return The position of the ith 1 or 0.
 */
uint64 bitvector::select(uint64& i, uint8_t byteValue) {
    return byteValue == 1 ? select_1(i) : select_0(i);
}

/**
 * Calculates the position of the num-th 0.
 * @param num The number of 0.
 * @return Its position.
 */
uint64 bitvector::select_0(uint64& num) {
    // If it's the last number, return cached position.
    if (num == zeroCount) return lastZeroPos;

    uint64 superblockIndex;
    // Each superblock has 128 bit of metadata, so 2 entries. start and end represent the actual superblock,
    // while the later superblock index represents the index in the vector, so essentially 2*superblock, or superblock >> 1.

    // First, check if either there is only one super block or the second super block already has too many 0s.
    if (superBlocks.size() <= 2 || num <= ((1 << 12) - (superBlocks[2] >> 20))) {
        superblockIndex = 0;
    } else if (num > (((superBlocks.size() - 2) << 11) - (superBlocks[superBlocks.size() - 2] >> 20))) {
        // Alternatively, check if it's the last block by seeing if the last superblock has still too few 0s.
        superblockIndex = superBlocks.size() - 2;
    } else {
        // The relevant super block is somewhere in the middle. Using the 0-cache, try establishing some loose boundaries
        // and perform a binary search.
        uint64 start, end;
        start = num >> 13;
        if (start >= selectCache_0.size()) {
            end = (superBlocks.size() >> 1) - 1;
        } else {
            // Start and end are exclusive in edge cases. By asserting that it's not the first or last
            // Super block, we can widen the search area just enough.
            end = selectCache_0[start] + 1;
            start = start == 0 ? 0 : selectCache_0[start - 1] - 1;
        }
        // Select Iterative returns the super block number, so multiply it by two to get the index.
        superblockIndex = (select_0_iterative(num, start, end) << 1); // Get super block before that. Guaranteed to be >= 0, since the case "is first block" is covered above.
    }

    // Inside the superblock, extract the metadata
    uint64 metadata1 = superBlocks[superblockIndex];
    uint64 metadata2 = superBlocks[superblockIndex + 1];

    // Since metadata stores only the amount of 1s, the following sections have a lot of totalBits - oneBits to
    // accurately calculate the amount of 0-bits without needing to store them.

    // Number of 0s = Number of Bits - Number of 1s, since bits are either 1 or 0.
    // To calculate total amount of bits, shift superblock by 1 to balance the *2 and shift by 12 to go from superblock to bit scale
    uint64 remaining = num - ((superblockIndex << 11) - (metadata1 >> 20));
    if ((superblockIndex << 11) > L0BLOCK_SIZE) remaining -= L0BLOCK_SIZE - L0SingleBlockData; // Also account for the second L0 block

    auto previousMetadataValue = 0ULL;
    auto currentMetadataValue = (BLOCK_SIZE - ((metadata1 >> 8) & 0xFFF));
    uint8_t blockIndex = 0;
    uint16_t blockTotalBitsTillHere = BLOCK_SIZE;

    // Next, we go through all the blocks inside the super block and see if at the end of that block, there would be more
    // 0s than what we're looking for. If so, we know that the num-th 0 is inside that block, since before it was too few.

    // Here, we use a do-while(false) to make the control flow a tad more efficient.
    // As soon as one of the break conditions is true, we subtract the metadata from the previous block and go to
    // after the loop, without the need to evaluate anything after.
    // This avoids the usage of go-to and some unnecessary if-evaluations. The only other way to get this with ifs is
    // multiple encapsulated ifs inside one another, which is not readable or nice.
    do {
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - (((metadata1 & 0xFF) << 4) | (metadata2 >> 60));
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - (metadata2 & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - ((metadata2 >> (12)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - ((metadata2 >> (24)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - ((metadata2 >> (36)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        blockTotalBitsTillHere += BLOCK_SIZE;
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = blockTotalBitsTillHere - ((metadata2 >> (48)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        remaining -= currentMetadataValue;
        ++blockIndex;

        // Last one is irrelevant as we know it has to be there (Ger.: Ausschlussverfahren)
    } while (false);


    // Now, go through a maximum of 8 64-bit words

    // First we calculate where the 64-bit word we're looking for is in the bitvector vector, and define the final position
    // as the position at the beginning of the block we're in.
    auto wordIndex = (superblockIndex << 5) + (blockIndex << 3);
    auto finalPosition = wordIndex << 6;
    uint8_t zerosInWord;

    // Loop over a maximum of 8 64-bit words.
    for (uint8_t index = 0; index < 8; ++index) {
        // Invert the word to count zeros using popcount.
        auto bitword = ~vector[wordIndex + index];
        zerosInWord = std::popcount(bitword);
        if (remaining > zerosInWord) {
            remaining -= zerosInWord;
            continue;
        }

        // Get position of the one in the word.
        // Unfortunately, I am doing a lot of testing on Windows and my Macbook, and my Macbook does not support
        // the trick with pdep and trailing zero count because of its processor architecture.
        // To make this happen anyway, we'll need to do the old classic way

        // This should be 0 if the loop is never run, but -1 otherwise because of the too early increment.
        // If the loop will trigger, the last statement will evaluate to 1, so bitIndex will become -1,
        // compensating for the first increment as planned. If not, 0.
        uint8_t bitIndex = 0 - (remaining > 0);

        while (remaining > 0) {
            remaining -= bitword & 1;
            bitword >>= 1;
            ++bitIndex;
        }

        // Now construct the final index from all the pieces, index should be shifted to cover the bits from 2^7 to 2^9,
        // The index of the word inside the block.
        finalPosition += index << 6;
        finalPosition += bitIndex;
        break;
    }

    // Return the constructed final position.
    return finalPosition;
}

/**
 * Helper function to perform the binary search for zeros. Takes in the position, the super block number (not index!)
 * of the start and end of the search area.
 * @param num The num-th zero.
 * @param start The start super block number.
 * @param end The end super block number.
 * @return The super block number of where the num-th zero is.
 */
uint64 bitvector::select_0_iterative(uint64& num, uint64 start, uint64 end) {
    unsigned long long middle;

    // Find the first super block where the number of zeros exceeds num.
    while (end > start) {
        middle = (start + end) >> 1;
        if (((middle << 12) - superRank(middle)) < num) {
            start = middle + 1;
        } else {
            end = middle;
        }
    }
    // start is the first super block where in the metadata it says 1s >= num. The superblock where it happens
    // is the one we want, so the super block before it.
    // Because of from where it's called, the start value here is >= 1.
    return start - 1;
}

/**
 * Calculates the position of the num-th 1.
 * @param num The number of 1.
 * @return Its position.
 */
uint64 bitvector::select_1(uint64& num) {
    // If it's the last number, return cached position.
    if (num == oneCount) return lastOnePos;

    uint64 superblockIndex;
    // Each superblock has 128 bit of metadata, so 2 entries. start and end represent the actual superblock,
    // while the later superblock index represents the index in the vector, so essentially 2*superblock, or superblock >> 1.

    // Very analogue to select_0, check if either there is only one super block or the second super block already has too many 1s.
    if (superBlocks.size() <= 2 || num <= (superBlocks[2] >> 20)) {
        superblockIndex = 0;
    } else if (num > (superBlocks[superBlocks.size() - 2] >> 20)) {
        // Then check if it's the last super block instead.
        superblockIndex = superBlocks.size() - 2;
    } else {
        // If also not, perform a binary search after setting the search area with a comfortable margin.
        uint64 start, end;
        start = num >> 13;
        if (start + 1 >= selectCache_1.size()) {
            end = (superBlocks.size() >> 1) - 1;
        } else {
            // Start and end are exclusive in edge cases. By asserting that it's not the first or last
            // Super block, we can widen the search area just enough.
            end = selectCache_1[start] + 1;
            start = start == 0 ? 0 : selectCache_1[start - 1] - 1;
        }
        // Get super block before that. Guaranteed to be >= 0, since the case "is first block" is covered above.
        superblockIndex = (select_1_iterative(num, start, end) << 1);
    }

    // Inside the superblock, get the metadata and how many 1s are remaining now.
    // Here, we do not need to invert any values, so this will be a bit nicer to look at.
    uint64 metadata1 = superBlocks[superblockIndex];
    uint64 metadata2 = superBlocks[superblockIndex + 1];
    uint64 remaining = num - (metadata1 >> 20);
    if ((superblockIndex << 11) > L0BLOCK_SIZE) remaining -= L0SingleBlockData; // Also account for the second L0 block

    auto previousMetadataValue = 0ULL;
    auto currentMetadataValue = ((metadata1 >> 8) & 0xFFF);
    uint8_t blockIndex = 0;

    // Here, we again use a do-while(false) to make the control flow a tad more efficient.
    // This avoids the usage of go-to and some unnecessary if-evaluations. The only other way to get this with ifs is
    // multiple encapsulated ifs inside one another, which is not readable or nice.
    do {
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = ((metadata1 & 0xFF) << 4) | (metadata2 >> 60);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = (metadata2 & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = ((metadata2 >> (12)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = ((metadata2 >> (24)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = ((metadata2 >> (36)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        previousMetadataValue = currentMetadataValue;
        ++blockIndex;

        currentMetadataValue = ((metadata2 >> (48)) & 0xFFF);
        if (currentMetadataValue >= remaining) {
            remaining -= previousMetadataValue;
            break;
        }
        remaining -= currentMetadataValue;
        ++blockIndex;

        // Last one is irrelevant as we know it has to be there (Ger.: Ausschlussverfahren)
    } while (false);

    // Now, go through a maximum of 8 64-bit words

    // First we calculate where the 64-bit word we're looking for is in the bitvector vector, and define the final position
    // as the position at the beginning of the block we're in.
    auto wordIndex = (superblockIndex << 5) + (blockIndex << 3);
    auto finalPosition = wordIndex << 6;
    uint8_t onesInWord;

    // Go through the 8 words or less
    for (uint8_t index = 0; index < 8; ++index) {
        auto bitword = vector[wordIndex + index];
        onesInWord = std::popcount(bitword);
        if (remaining > onesInWord) {
            remaining -= onesInWord;
            continue;
        }

        // Get position of the one in the word.
        // Unfortunately, I am doing a lot of testing on Windows and my Macbook, and my Macbook does not support
        // the trick with pdep and trailing zero count because of its processor architecture.
        // To make this happen anyway, we'll need to do the old classic way

        // This should be 0 if the loop is never run, but -1 otherwise because of the too early increment.
        // If the loop will trigger, the last statement will evaluate to 1, so bitIndex will become -1,
        // compensating for the first increment as planned. If not, 0.
        uint8_t bitIndex = 0 - (remaining > 0);

        while (remaining > 0) {
            remaining -= bitword & 1;
            bitword >>= 1;
            ++bitIndex;
        }

        // Now construct the final index from all the pieces
        finalPosition += index << 6;
        finalPosition += bitIndex;
        break;
    }

    // Return the final position of the num-th 1.
    return finalPosition;
}

/**
 * Helper function to perform the binary search for ones. Takes in the position, the super block number (not index!)
 * of the start and end of the search area.
 * @param num The num-th one.
 * @param start The start super block number.
 * @param end The end super block number.
 * @return The super block number of where the num-th one is.
 */
uint64 bitvector::select_1_iterative(uint64& num, uint64 start, uint64 end) {
    uint64 middle;

    // Find the first super block where the number of ones exceeds num.
    while (end > start) {
        middle = (start + end) >> 1;
        if (superRank(middle) < num) {
            start = middle + 1;
        } else {
            end = middle;
        }
    }
    // start is the first super block where in the metadata it says 1s >= num. The superblock where it happens
    // is the one we want, so the super block before it.
    // Because of from where it's called, the start value here is >= 1.
    return start - 1;
}

/**
 * This builds all helper data structure for the bitvector. Please call this method before performing any rank or select query.<br/>
 * We loop through the vector one 64-bit word at a time. Since the edges for blocks and super blocks are a multiple of 64, the
 * order of the 1s and 0s in the words does not matter. Thus, the ones can be counted using popcount, which is significantly faster
 * than iterating over the bits.<br/>
 * Since we keep track of the last one and zero position, we need to update it about once per 64-bit word each. While this is not
 * really that great, it doesn't have much of an impact on performance, so we're good. These values are cached because the select
 * queries can get a little bit fussy with certain edge cases when the position of the last 1 or 0 is requested. To evade this entirely,
 * we just cache the values, which results in 128 bit additional overhead, which is okay compared to a bitvector size in the thousands
 * or millions.
 */
void bitvector::buildHelpers() {

    superBlocks = *new std::vector<uint64>(((vector.size() >> 6) << 1) + 2ULL);    // 1 superblock covers 4096 (2^12) bit, but needs 2*64 bit. So, multiply amount by 2.

    // Per L0, we have 2^31 superblock indices. The 32nd bit is automatically the index of the L0 block.
    // This is more than enough to cover all 2^64 positions reachable using the 64-bit indices.
#define BLOCKS_IN_SUPERBLOCK 8
#define WORDS_IN_BLOCK 8
#define SUPERBLOCKS_PER_L0 0x7FFFFFFF // 2 ^ 31

    // Initialize lots of counters
    uint8_t wordIndex = 0, blockIndex = 0, onesInBlock;
    uint64 metadata1Builder = 0, metadata2Builder = 0;
    uint64 L0BlockOneCounter = 0, superBlockOneCounter = 0, superblockIndex = 0;

    // Initialize the next threshold value for saving the superblock to the select cache
    uint64 nextSelectCacheThreshold_0 = EVERY_OTHER_1_POS, nextSelectCacheThreshold_1 = EVERY_OTHER_1_POS;
    uint64 word;

    for (uint64 vectorIndex = 0; vectorIndex < vector.size(); ++vectorIndex) {
        // Read and process the next word first.
        word = vector[vectorIndex];

        ++wordIndex;
        onesInBlock = std::popcount(word);
        superBlockOneCounter += onesInBlock;
        L0BlockOneCounter += onesInBlock;
        oneCount += onesInBlock;
        zeroCount += 64 - onesInBlock;

        // Update the last 0 and 1 position when one occurs. We need to do this every time, since the last
        // one or zero position is arbitrary.
        if (onesInBlock > 0) {
            // The Position at the start of the word + the position of the first 1. 63 - number of leading zeros.
            lastOnePos = (vectorIndex << 6) + (63 - __builtin_clzll(word));
        }
        if (onesInBlock < 64) {
            // same as above but inverted if at least one zero is in the word.
            lastZeroPos = (vectorIndex << 6) + (63 - __builtin_clzll(~word));
        }

        if (oneCount >= nextSelectCacheThreshold_1) {
            nextSelectCacheThreshold_1 += EVERY_OTHER_1_POS;
            selectCache_1.emplace_back(superblockIndex >> 1);
        }
        if (zeroCount >= nextSelectCacheThreshold_0) {
            nextSelectCacheThreshold_0 += EVERY_OTHER_1_POS;
            selectCache_0.emplace_back(superblockIndex >> 1);
        }

        // Update superblock metadata and indices. Do this when finishing a block, superblock,
        // or if this is the last loop iteration. If we do not save the last collected metadata,
        // this can lead to several bugs with rank and select.
        // This is also the reason we are not using an enhanced for-loop (for-each).
        if (wordIndex == WORDS_IN_BLOCK || vectorIndex == vector.size() - 1) {
            wordIndex = 0;
            if (blockIndex == BLOCKS_IN_SUPERBLOCK - 1) {

                // Push metadata
                superBlocks[superblockIndex++] = metadata1Builder;
                superBlocks[superblockIndex++] = metadata2Builder;

                // If this happens to be the change from the first to the second L0 block, save
                // the amount of 1s
                if ((superblockIndex >> 1) == SUPERBLOCKS_PER_L0) {
                    L0SingleBlockData = oneCount;
                    L0BlockOneCounter = 0;
                }

                // Reset values:
                metadata1Builder = L0BlockOneCounter << 20;  // Push all previous ones here. This is on purpose.
                metadata2Builder = 0;
                superBlockOneCounter = 0;
                blockIndex = 0;

            } else {
                // Not a superblock, but instead a block. Save this to the current metadata.
                if (blockIndex == 0) {
                    metadata1Builder |= (superBlockOneCounter & 0xFFF) << 8;
                } else if (blockIndex == 1) {
                    metadata1Builder |= ((superBlockOneCounter >> 4) & 0xFF);
                    metadata2Builder |= (superBlockOneCounter & 0xF) << 60;
                } else {
                    metadata2Builder |= (superBlockOneCounter & 0xFFF) << ((blockIndex - 2) * 12);
                }
                ++blockIndex;
            }
        }
    }

    // If there is still space (there should be, but in edge cases we don't want to write on unknown memory),
    // push the last unfinished metadata to the superblocks vector as well. This is important information on the
    // blocks and the existance of a superblock there, as the superblocks should cover the entire vector.
    if (superblockIndex < superBlocks.size()) {
        superBlocks[superblockIndex++] = metadata1Builder;
        superBlocks[superblockIndex++] = metadata2Builder;
    }
}

/**
 * This calculates how much bit in total the bitvector object is taking up.
 * This includes all lists, vectors, and static overhead.
 * @return The space usage in bit.
 */
uint64 bitvector::size() {
    // 5 * 64 bit through misc metadata: L0SingleBlockData, zeroCount, oneCount, last one and zero position
    uint64 size = 320;

    size += vector.capacity() * 64;
    size += superBlocks.capacity() * 64;
    size += selectCache_0.capacity() * 32;
    size += selectCache_1.capacity() * 32;

    return size;
}