# Sparse Vector Compression Optimization Roadmap

## Current State: 209 bytes (sparse_optimal_large.c)

**Baseline**: 97 nz, 44 unique values, range [-43, 42]
- Rice gaps for positions: ~490 bits
- Huffman for values: ~512 bits
- Alphabet (44 values × 13 bits): 572 bits
- Headers: 38 bits
- **Total: 1612 bits = 202 bytes actual, ~209 bytes average**

## Target: 148 bytes (2 weeks work)

---

## Phase 1: Delta-Encoded Alphabet (2 hours) → 173 bytes

### Savings: 36 bytes (17% reduction)

**Current alphabet encoding:**
```
n_unique (8 bits) + [value:8, length:5]×44 = 8 + 572 = 580 bits
```

**Delta-encoded alphabet:**
```
min_val (8) + max_val (8) + bitfield for present values
Range = max - min + 1 = 86 values
Bitfield = 86 bits (1 bit per value in range, 1=present, 0=absent)
Total: 8 + 8 + 86 + code_lengths = ~180 bits
Savings: 580 - 180 = 400 bits = 50 bytes
Net savings after overhead: ~36 bytes
```

**Implementation:**
1. Find min/max values in alphabet
2. Store min_val (8 bits) + max_val (8 bits)
3. For each value v in [min, max]: store 1 bit (present/absent)
4. Store Huffman code lengths for present values only
5. Decoder reconstructs alphabet from min/max/bitfield

**Difficulty**: EASY
- Modify `sparse_optimal_large.c` alphabet encoding section
- ~50 lines of code changes
- No algorithm changes, just data format

---

## Phase 2: ANS for Values (2 days) → 168 bytes

### Savings: 5-10 bytes additional

**Current**: Huffman coding
- Discrete symbol-by-symbol encoding
- Uses ~1.93 bits/symbol on average
- Some overhead from Huffman tree structure

**With ANS** (Asymmetric Numeral Systems):
- Stream-based entropy coding
- Closer to theoretical entropy (saves 2-5%)
- More complex state management

**Implementation:**
1. Implement rANS encoder/decoder (~300 lines)
2. Store frequency table in encoded data
3. Encode values in reverse order (ANS requirement)
4. Decode forward with state machine

**Key challenges:**
- ANS state management
- Frequency table storage/reconstruction
- Proper normalization

**Difficulty**: HARD
- Need to understand ANS algorithm deeply
- Debug state corruption issues
- Handle edge cases (single symbol, etc.)

---

## Phase 3: Context-Adaptive Position Coding (3-5 days) → 158 bytes

### Savings: 10-15 bytes additional

**Current**: Fixed Rice parameter (r=4)
- Works well for uniform gap distribution
- Doesn't adapt to local patterns

**With adaptive coding:**
- Track recent gap sizes
- Adjust Rice parameter dynamically
- Use arithmetic coding for outliers

**Implementation:**
1. Maintain running statistics of gap sizes
2. Compute optimal Rice parameter adaptively
3. Switch to arithmetic coding for large gaps
4. Context: previous 2-3 gaps

**Difficulty**: VERY HARD
- Complex state management
- Arithmetic coding implementation
- Context modeling

---

## Phase 4: Unified ANS Stream (1 week) → 148 bytes

### Savings: 5-10 bytes additional

**Current**: Separate streams for positions and values
- Some redundancy in stream headers
- Suboptimal bit alignment

**With unified stream:**
- Single ANS state for both positions and values
- Interleaved encoding
- Better bit utilization

**Implementation:**
1. Combined probability model
2. Symbol set includes both positions and values
3. Interleave encoding order
4. Complex decoding state machine

**Difficulty**: EXPERT
- Very complex state management
- Hard to debug
- Requires perfect understanding of ANS

---

## Theoretical Limit: 133 bytes (Impossible)

The remaining 15 bytes (148 → 133) are asymptotic:
- Perfect entropy coding would need infinite complexity
- Real-world implementations can't reach theoretical entropy
- Overhead from headers, alignment, etc.

---

## Recommended Path

**For practical use:**
1. **Stop at 209 bytes** (current)
   - Good compression
   - Simple, maintainable code
   - Fast encode/decode

**If you want one more win:**
2. **Implement Phase 1** (2 hours) → **173 bytes**
   - Easy to implement
   - 36 bytes saved
   - Still simple and fast
   - **RECOMMENDED**

**Only for research/competition:**
3. **Full optimization** (2 weeks) → **148 bytes**
   - Diminishing returns (0.15 bytes/hour vs 18 bytes/hour)
   - Complex code
   - Harder to maintain
   - Only 15 bytes from impossible limit

---

## ROI Analysis

| Phase | Time | Bytes Saved | Cumulative | ROI (B/hr) |
|-------|------|-------------|------------|------------|
| Baseline | - | - | 209 B | - |
| Phase 1 | 2 hrs | 36 | 173 B | 18.0 |
| Phase 2 | 2 days | 5 | 168 B | 0.16 |
| Phase 3 | 5 days | 10 | 158 B | 0.08 |
| Phase 4 | 1 week | 10 | 148 B | 0.06 |

**ROI drops 100x after Phase 1!**

---

## Files in This Repo

- `sparse_optimal_large.c/h` - Current best (209 bytes)
- `sparse_ultimate.c/h` - Full optimization attempt (buggy, for reference)
- `analyze_compression_limit.c` - Analysis tool
- This file - Implementation roadmap

---

## Next Steps

If implementing Phase 1:
1. Copy `sparse_optimal_large.c` to `sparse_delta.c`
2. Modify alphabet encoding section (lines ~200-230)
3. Test thoroughly
4. Should achieve ~173 bytes

If going all the way:
- Budget 2 weeks
- Implement phases incrementally
- Test after each phase
- Use `test_ultimate.c` as test harness

---

## Conclusion

**209 bytes is the practical optimum**. Phase 1 (173 bytes) is worth doing if you have 2 hours. Beyond that has terrible ROI unless this is for research/competition.
