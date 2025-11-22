# Batch Encoding: Multi-Vector rANS

## Summary

**Batch encoding amortizes rANS overhead across multiple vectors, making it superior to Phase 1 for batches of 5+ vectors.**

| Batch Size | Phase 1 (Huffman) | Phase 2 (Individual rANS) | Batch rANS | Winner |
|------------|-------------------|---------------------------|------------|--------|
| N=1  | 141.0 bytes/vec | 155.0 bytes/vec | 161.0 bytes/vec | **Phase 1** ✓ |
| N=2  | 137.5 bytes/vec | 150.5 bytes/vec | 139.5 bytes/vec | **Phase 1** ✓ |
| N=5  | 139.2 bytes/vec | 152.8 bytes/vec | **129.2 bytes/vec** | **Batch** ✓ |
| N=10 | 141.3 bytes/vec | 155.3 bytes/vec | **125.7 bytes/vec** | **Batch** ✓ |
| N=20 | 140.1 bytes/vec | 153.8 bytes/vec | **122.5 bytes/vec** | **Batch** ✓ |
| N=50 | 139.3 bytes/vec | 153.0 bytes/vec | **120.2 bytes/vec** | **Batch** ✓ |

**Key Findings:**
- ✅ **N ≥ 5**: Batch encoding beats Phase 1 by 10-19 bytes/vector
- ✅ **N=10**: 126 bytes/vec vs Phase 1's 141 bytes/vec (**11% reduction**)
- ✅ **N=50**: 120 bytes/vec vs Phase 1's 139 bytes/vec (**14% reduction**)
- ❌ **N < 5**: Phase 1 (Huffman) is still better due to lower overhead

## How It Works

**Phase 1 (Huffman) - Per Vector:**
```
[Header] [Alphabet] [Huffman codes] [Positions] [Values]
  ~4B      ~8B         ~15B           ~50B       ~65B
                  ↑ Low overhead (15B) ↑
```

**Phase 2 (rANS) - Per Vector:**
```
[Header] [Alphabet] [Freq table] [Positions] [rANS stream] [State]
  ~4B      ~8B         ~30B         ~50B        ~55B         4B
                  ↑ High overhead (34B) ↑
```

**Batch Encoding - N Vectors:**
```
[Header] [Shared Alphabet] [Shared Freq] [N × Positions] [Shared rANS] [Shared State]
  ~6B         ~8B              ~30B         N × ~50B         N × ~55B        4B
          ↑ Overhead paid ONCE (42B) instead of N × 34B ↑
```

## Overhead Breakdown

**Single vector encoding:**
- Phase 1: 15 bytes overhead → **141 bytes total**
- Phase 2: 34 bytes overhead → **155 bytes total**
- Difference: **+14 bytes** (Phase 2 worse)

**10 vectors individually (Phase 2):**
- Overhead: 34 bytes × 10 = **340 bytes**
- Total: **1553 bytes** (155.3 bytes/vec)

**10 vectors batched:**
- Overhead: 42 bytes (shared) = **42 bytes**
- Total: **1257 bytes** (125.7 bytes/vec)
- **Savings: 296 bytes** (19% reduction vs individual)

## When to Use Each Method

### Use Phase 1 (sparse_delta.c):
- ✅ Single vectors
- ✅ Small batches (N < 5)
- ✅ Simplicity is important
- **Size: ~141 bytes/vector**

### Use Batch Encoding (sparse_batch.c):
- ✅ Multiple vectors (N ≥ 5)
- ✅ All vectors have same dimension
- ✅ Can encode/decode as a batch
- **Size: ~129-120 bytes/vector** (N=5-50)

### DON'T Use Phase 2 Individual:
- ❌ Always worse than either Phase 1 or Batch
- **Size: ~155 bytes/vector** (worst of both worlds)

## API

### Encoding
```c
// Encode N vectors into a single batch
const int8_t *vectors[N];  // Array of vector pointers
sparse_batch_t *batch = sparse_batch_encode(vectors, N, dimension);

// Result: batch->size bytes (amortized overhead)
```

### Decoding
```c
// Decode batch into N vectors
int8_t *output[N];  // Pre-allocated array of vectors
sparse_batch_decode(batch, output, N, dimension);

// All N vectors decoded in one operation
```

## Implementation Details

**Shared Components:**
1. **Single frequency table** (30 bytes) - shared across all N vectors
2. **Single rANS stream** - all values encoded together
3. **Single final state** (4 bytes) - shared decoder state

**Per-Vector Components:**
1. **Position count** (2 bytes per vector)
2. **Position encoding** (~50 bytes per vector)

**Encoding order:**
- Positions: encoded per-vector in forward order
- Values: all values encoded together in reverse order (rANS requirement)

**Decoding order:**
- Positions: decoded per-vector
- Values: decoded in forward order (FIFO from reversed encoding)

## Test Results

**Correctness: ✓ 100% success**
```
Test: 10 vectors × 2048 dimensions
✓ All 10 vectors decoded correctly
✓ Perfect reconstruction of all values
```

**Performance:**
```
Individual encoding:  1553 bytes total
Batch encoding:       1257 bytes total
Savings:              296 bytes (19.1%)
```

**Scaling (bytes per vector):**
```
N=1:  Phase1=141  Batch=161  → Phase1 better by 20
N=5:  Phase1=139  Batch=129  → Batch better by 10 ✓
N=10: Phase1=141  Batch=126  → Batch better by 15 ✓
N=50: Phase1=139  Batch=120  → Batch better by 19 ✓
```

## Theoretical Analysis

**Overhead per vector:**
- Phase 1: 15 bytes (Huffman code table)
- Phase 2: 34 bytes (frequency table + state)
- Batch: 42/N bytes (amortized)

**Crossover point:**
```
When does batch beat Phase 1?
  141 = (42/N) + data
  139.2 = (42/5) + data  → N=5 is breakeven

For N ≥ 5: batch wins
```

**Optimal batch size:**
- Larger is better (overhead → 0 as N → ∞)
- Practical: N=10-50 provides most benefit
- Diminishing returns beyond N=100

## Files

- `sparse_batch.h` - Batch encoding API
- `sparse_batch.c` - Implementation (~500 lines)
- `test_batch.c` - Basic test (10 vectors)
- `test_batch_scaling.c` - Scaling test (1-50 vectors)

## Conclusion

**Batch encoding solves the rANS overhead problem!**

By sharing the frequency table and rANS state across multiple vectors:
- ✅ Reduces overhead from 34 bytes/vec to 42/N bytes/vec
- ✅ Beats Phase 1 (Huffman) for N ≥ 5 vectors
- ✅ 11-14% smaller than Phase 1 for typical batches (N=10-50)
- ✅ 100% correct reconstruction

**Recommendation:**
- **Single vectors**: Use Phase 1 (141 bytes)
- **Batches of 5+**: Use batch encoding (120-129 bytes/vec)
- **Phase 2 individual**: Deprecated (always worse)

This makes rANS practical for sparse vector compression! 🎉
