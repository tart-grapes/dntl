# NTT Prime Numbers for Various Dimensions

This document contains the smallest primes suitable for NTT (Number Theoretic Transform) operations at various dimensions. All primes satisfy the requirements for **negacyclic NTT**: `p ≡ 1 (mod 2N)`.

## Requirements

For a dimension N:
- **Cyclic NTT**: requires p ≡ 1 (mod N)
- **Negacyclic NTT**: requires p ≡ 1 (mod 2N)

All primes listed satisfy:
1. Primality (verified with Miller-Rabin test)
2. Congruence: p ≡ 1 (mod 2N)
3. Existence of primitive 2N-th root of unity ψ with ψ^N = -1
4. Existence of N-th root of unity ω = ψ²

## N = 1024 (10-bit dimension)

| # | Prime   | Bits | g  | ψ (2N-th root) | ω (N-th root) |
|---|---------|------|----|--------------:|-------------:|
| 1 | 12,289  | 14   | 11 | 1,945         | 10,302       |
| 2 | 18,433  | 15   | 5  | 17,660        | 7,673        |
| 3 | 40,961  | 16   | 3  | 20,237        | 8,091        |
| 4 | 59,393  | 16   | 5  | 53,350        | 50,547       |
| 5 | 61,441  | 16   | 17 | 16,290        | 421          |
| 6 | 65,537  | 17   | 3  | 61,869        | 19,139       |
| 7 | 79,873  | 17   | 7  | 13,725        | 35,091       |
| 8 | 83,969  | 17   | 3  | 27,450        | 48,663       |
| 9 | 86,017  | 17   | 5  | 67,536        | 59,871       |
| 10| 114,689 | 17   | 3  | 112,622       | 28,996       |

## N = 2048 (11-bit dimension)

| # | Prime   | Bits | g  | ψ (2N-th root) | ω (N-th root) |
|---|---------|------|----|--------------:|-------------:|
| 1 | 12,289  | 14   | 11 | 1,331         | 1,945        |
| 2 | 40,961  | 16   | 3  | 18,088        | 20,237       |
| 3 | 61,441  | 16   | 17 | 39,003        | 16,290       |
| 4 | 65,537  | 17   | 3  | 54,449        | 61,869       |
| 5 | 86,017  | 17   | 5  | 7,923         | 67,536       |
| 6 | 114,689 | 17   | 3  | 6,932         | 112,622      |
| 7 | 147,457 | 18   | 10 | 129,927       | 512          |
| 8 | 151,553 | 18   | 3  | 52,786        | 59,891       |
| 9 | 163,841 | 18   | 3  | 34,932        | 120,697      |
| 10| 176,129 | 18   | 3  | 141,504       | 156,651      |

## N = 4096 (12-bit dimension)

| # | Prime   | Bits | g  | ψ (2N-th root) | ω (N-th root) |
|---|---------|------|----|--------------:|-------------:|
| 1 | 40,961  | 16   | 3  | 243           | 18,088       |
| 2 | 65,537  | 17   | 3  | 6,561         | 54,449       |
| 3 | 114,689 | 17   | 3  | 80,720        | 6,932        |
| 4 | 147,457 | 18   | 10 | 62,093        | 129,927      |
| 5 | 163,841 | 18   | 3  | 84,080        | 34,932       |
| 6 | 188,417 | 18   | 3  | 59,526        | 162,991      |
| 7 | 270,337 | 19   | 10 | 86,290        | 72,109       |
| 8 | 286,721 | 19   | 11 | 173,352       | 261,336      |
| 9 | 319,489 | 19   | 23 | 80,551        | 280,989      |
| 10| 417,793 | 19   | 5  | 153,728       | 254,732      |

## N = 8192 (13-bit dimension)

| # | Prime     | Bits | g  | ψ (2N-th root) | ω (N-th root) |
|---|-----------|------|----|--------------:|-------------:|
| 1 | 65,537    | 17   | 3  | 81            | 6,561        |
| 2 | 114,689   | 17   | 3  | 2,187         | 80,720       |
| 3 | 147,457   | 18   | 10 | 94,083        | 62,093       |
| 4 | 163,841   | 18   | 3  | 59,049        | 84,080       |
| 5 | 557,057   | 20   | 3  | 446,794       | 160,144      |
| 6 | 638,977   | 20   | 7  | 461,405       | 217,165      |
| 7 | 737,281   | 20   | 11 | 667,463       | 388,433      |
| 8 | 786,433   | 20   | 10 | 83,833        | 406,601      |
| 9 | 1,032,193 | 20   | 11 | 445,136       | 97,058       |
| 10| 1,097,729 | 21   | 3  | 489,228       | 693,469      |

## N = 16384 (14-bit dimension)

| # | Prime     | Bits | g  | ψ (2N-th root) | ω (N-th root) |
|---|-----------|------|----|--------------:|-------------:|
| 1 | 65,537    | 17   | 3  | 9             | 81           |
| 2 | 163,841   | 18   | 3  | 243           | 59,049       |
| 3 | 557,057   | 20   | 3  | 459,996       | 446,794      |
| 4 | 786,433   | 20   | 10 | 585,160       | 83,833       |
| 5 | 1,146,881 | 21   | 3  | 1,118,503     | 200,422      |
| 6 | 1,179,649 | 21   | 19 | 16,531        | 775,042      |
| 7 | 1,376,257 | 21   | 5  | 845,682       | 590,046      |
| 8 | 1,769,473 | 21   | 5  | 1,651,728     | 64,070       |
| 9 | 2,424,833 | 22   | 3  | 1,283,767     | 563,175      |
| 10| 2,654,209 | 22   | 11 | 1,985,530     | 902,692      |

## Notable Primes

Some primes appear across multiple dimensions:

- **65,537** (Fermat prime F₄): Works for N ≥ 4096
- **12,289**: Works for N ∈ {1024, 2048}
- **40,961**: Works for N ∈ {1024, 2048, 4096}
- **163,841**: Works for N ≥ 2048

## Usage

To find more primes for a specific dimension:

```bash
./find_ntt_primes <dimension> <count>
```

Example:
```bash
./find_ntt_primes 4096 10
```

## Implementation Notes

The `find_ntt_primes.c` program:
1. Searches for primes satisfying p ≡ 1 (mod 2N)
2. Verifies primality using Miller-Rabin test (12 rounds)
3. Finds primitive root g modulo p
4. Computes and verifies ψ (2N-th root with ψ^N = -1)
5. Computes ω = ψ² (N-th root of unity)

All root-finding operations use constant-time modular arithmetic suitable for cryptographic applications.
