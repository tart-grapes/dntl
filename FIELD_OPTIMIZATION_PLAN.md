# Field Arithmetic Optimization Plan

## Current Implementation Analysis

### What We Have (ntt64.c)
- ✅ Barrett reduction with 128-bit intermediate arithmetic
- ✅ Branchless conditional subtraction for add/sub
- ✅ Constant-time execution
- ✅ Works correctly for all 8 layers

### Performance Characteristics
- **Small primes** (257-64513): Using 128-bit Barrett is overkill
- **Large prime** (4294955009): Could benefit from Montgomery arithmetic
- **All operations**: Already branchless and constant-time ✓

---

## Optimization Strategy

### 1. Small Prime Optimization (Layers 0-4)

**Primes**: 257, 3329, 12289, 40961, 64513

**Current**: Uses 128-bit Barrett reduction
```c
uint64_t q_approx = ((__uint128_t)x * b) >> 64;
```

**Optimized**: Use 32-bit Barrett reduction
```c
// Precompute: MU_SMALL[layer] = floor(2^32 / q)
uint32_t q_approx = (uint32_t)(((uint64_t)prod * mu) >> 32);
```

**Benefits**:
- No 128-bit arithmetic needed
- Faster on platforms without native 128-bit support
- Product a*b fits in 32 bits for all small primes

**Implementation**:
```c
static const uint32_t MU_SMALL[5] = {
    0xFFFFFF01u,  // floor(2^32 / 257)
    0xFFCF1BBBu,  // floor(2^32 / 3329)
    0xFFFD0001u,  // floor(2^32 / 12289)
    0xFFFEFFFFu,  // floor(2^32 / 40961)
    0xFFFEFC01u,  // floor(2^32 / 64513)
};

static inline uint32_t ct_mul_mod_small(uint32_t a, uint32_t b, int layer) {
    uint32_t q = Q[layer];
    uint32_t mu = MU_SMALL[layer];

    uint32_t prod = a * b;
    uint32_t q_est = (uint32_t)(((uint64_t)prod * mu) >> 32);
    uint32_t r = prod - q_est * q;

    // Branchless final reduction (max 2 iterations)
    uint32_t mask = -(r >= q);
    r -= mask & q;
    mask = -(r >= q);
    r -= mask & q;

    return r;
}
```

---

### 2. Montgomery Arithmetic for Large Prime (Layer 7)

**Prime**: 4294955009 (Q_big)

**Why Montgomery**:
- R = 2^32 is natural for 32-bit arithmetic
- Avoids 128-bit multiplication
- Single 64-bit multiply instead of 128-bit

**Precomputation**:
```c
// Q_big = 4294955009
// R = 2^32 = 4294967296
// QINV = -Q^(-1) mod R
// R2 = R^2 mod Q (for conversion)

static const uint32_t MONT_Q = 4294955009u;
static const uint32_t MONT_QINV = 12287u;  // -Q^(-1) mod 2^32
static const uint32_t MONT_R2 = 151060481u; // 2^64 mod Q
```

**Montgomery Multiplication**:
```c
// Input: a_mont, b_mont in Montgomery form (a*R mod Q, b*R mod Q)
// Output: (a*b*R) mod Q
static inline uint32_t montgomery_mul(uint32_t a_mont, uint32_t b_mont) {
    uint64_t t = (uint64_t)a_mont * b_mont;
    uint32_t m = (uint32_t)t * MONT_QINV;
    uint64_t u = (t + (uint64_t)m * MONT_Q) >> 32;

    uint32_t r = (uint32_t)u;
    uint32_t mask = -(r >= MONT_Q);
    return r - (mask & MONT_Q);
}
```

**Conversion**:
```c
// To Montgomery: a_mont = (a * R) mod Q = montgomery_mul(a, R2)
static inline uint32_t to_montgomery(uint32_t a) {
    return montgomery_mul(a, MONT_R2);
}

// From Montgomery: a = a_mont * R^(-1) mod Q = montgomery_mul(a_mont, 1)
static inline uint32_t from_montgomery(uint32_t a_mont) {
    return montgomery_mul(a_mont, 1);
}
```

**Add/Sub in Montgomery Domain**:
```c
// Addition is the same in Montgomery domain!
static inline uint32_t montgomery_add(uint32_t a_mont, uint32_t b_mont) {
    uint64_t sum = (uint64_t)a_mont + b_mont;
    uint32_t mask = -(sum >= MONT_Q);
    return (uint32_t)(sum - (mask & MONT_Q));
}

// Subtraction also the same
static inline uint32_t montgomery_sub(uint32_t a_mont, uint32_t b_mont) {
    int64_t diff = (int64_t)a_mont - b_mont;
    int64_t mask = -(diff < 0);
    return (uint32_t)(diff + (mask & MONT_Q));
}
```

**Benefits**:
- ~30-40% faster multiplication
- No 128-bit arithmetic
- Critical for NTT performance on layer 7

---

### 3. Medium Primes (Layers 5-6)

**Primes**: 786433, 2013265921

**Strategy**: Keep current 128-bit Barrett
- Product a*b needs up to 40 bits (786433^2) or 62 bits (2013265921^2)
- 128-bit Barrett is appropriate here
- Not worth separate optimization

---

## Implementation Structure

### Dispatch Strategy

```c
// Public API remains the same
uint32_t ntt64_mul_mod(uint32_t a, uint32_t b, int layer) {
    if (layer <= 4) {
        // Small prime optimization
        return ct_mul_mod_small(a, b, layer);
    } else if (layer == 7) {
        // Montgomery optimization (optional, requires conversion)
        // For now, could keep using Barrett unless we convert
        // all layer 7 code to Montgomery domain
        return ct_mul_mod(a, b, layer);
    } else {
        // Medium primes: use current Barrett
        return ct_mul_mod(a, b, layer);
    }
}
```

### Montgomery Domain Usage

Montgomery is most beneficial when:
1. **Doing many multiplications** (like in NTT)
2. **Can amortize conversion cost**

For NTT on layer 7:
```c
void ntt64_forward_layer7_montgomery(uint32_t poly[NTT_N]) {
    // Convert to Montgomery domain once
    for (int i = 0; i < NTT_N; i++) {
        poly[i] = to_montgomery(poly[i]);
    }

    // All twiddle factors also precomputed in Montgomery form
    // ... NTT butterfly operations using montgomery_mul ...

    // Convert back from Montgomery domain once
    for (int i = 0; i < NTT_N; i++) {
        poly[i] = from_montgomery(poly[i]);
    }
}
```

---

## Performance Estimates

### Current Implementation (All layers use 128-bit Barrett)
- Small prime multiply: ~8-10 cycles
- Large prime multiply: ~12-15 cycles

### With Optimizations
- Small prime multiply (32-bit Barrett): ~4-6 cycles (**~40% faster**)
- Large prime multiply (Montgomery): ~8-10 cycles (**~30% faster**)
- Add/sub: No change (already optimal at ~2-3 cycles)

### Where It Matters Most
1. **NTT butterflies**: Dominated by multiplications
2. **Point-wise multiplication**: Pure multiplications
3. **Layer 7 operations**: Largest prime, most benefit from Montgomery

---

## Implementation Status

### Phase 1: Small Prime Optimization - ❌ NOT FEASIBLE
**Reason**: The NTT implementation passes potentially unreduced values to `ct_mul_mod()`, meaning inputs `a` and `b` might be ≥ q. This requires handling 64-bit products even for small primes, which negates the benefit of 32-bit Barrett reduction.

**What was attempted**:
- ✅ Computed MU_SMALL constants for 32-bit Barrett
- ✅ Implemented ct_mul_mod_small() for products < 2^32
- ❌ Failed: NTT butterfly operations don't guarantee reduced inputs
- ❌ Failed: 32-bit Barrett can't handle 64-bit products accurately

**Conclusion**: The current 128-bit Barrett implementation is already optimal for this use case. It correctly handles unreduced inputs and 64-bit products with constant-time guarantees.

### Phase 2: Montgomery for Layer 7 (Feasible but requires NTT rewrite)
**Status**: Not implemented

**Requirements**:
- [ ] Compute MONT_QINV and MONT_R2
- [ ] Implement montgomery_mul/add/sub
- [ ] **Rewrite NTT to maintain Montgomery domain throughout**
  - Convert inputs to Montgomery at start of forward NTT
  - Precompute twiddle factors in Montgomery form
  - All butterflies work in Montgomery domain
  - Convert back from Montgomery at end of inverse NTT

**Expected Benefit**: ~30-40% faster for layer 7 operations, but requires significant NTT code changes.

**Trade-off**: Code complexity vs. performance gain. Current implementation is already fast and constant-time.

### Phase 3: Alternative Optimizations (Recommended)
Instead of micro-optimizing field arithmetic, consider:

- [x] **Use compiler optimizations**: `-O2` or `-O3` already optimize away much overhead
- [ ] **SIMD vectorization**: Process multiple NTT butterflies in parallel
- [ ] **Cache optimization**: Reorder NTT stages for better cache locality
- [ ] **Platform-specific**: Use AVX2/AVX-512 for x86, NEON for ARM
- [ ] **Lazy reduction**: Keep values in range [0, 2q) to reduce reduction overhead

---

## Notes

### Division = Multiply by Inverse
- Never do actual division in hot paths
- Always use: `a/b mod q` = `a * b^(-1) mod q`
- Inverse computation (ntt64_inv_mod) is slow but only needed:
  - During precomputation (twiddle factors, etc.)
  - For occasional "division" operations

### Constant-Time Guarantees
All optimizations maintain constant-time execution:
- No data-dependent branches
- Fixed iteration counts
- Branchless conditionals using masks

### Compatibility
All optimizations are internal:
- Public API unchanged
- Existing code continues to work
- Can enable optimizations incrementally
