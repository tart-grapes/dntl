#!/usr/bin/env python3
import numpy as np
import argparse
import os
from random import choice
import gmpy2
import hashlib


parser = argparse.ArgumentParser(description="DNTL-DSA")
xof = hashlib.shake_256
parser.add_argument("-c", required=False, default=1, type=int, help="Config block")

args = parser.parse_args()
CRange = np.concatenate((np.arange(-127, 0, dtype=int), np.arange(1, 128, dtype=int)))

# CONSTANT_BLOCK
if args.c == 5:
    # instances
    K = 3
    # DIMENSION
    N = 128
    A_VEC = N-2
    Q = 257
    R = 3
    Q2 = 257
    R2 = 5
    X = 0
    SEED_SIZE = 32
    if X == 0:
        allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        max_norm = 22.627416998
    sigma = 2

if args.c == 3:
    # Instances
    K = 3
    # DIMENSION
    N = 64
    n = N
    A_VEC = N-2

    Q = 257
    R = 3
    Q2 = 257
    R2 = 5
    X = 0
    SEED_SIZE = 24
    if X == 0:
        allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        max_norm = 16
    sigma = 2


if args.c == 1:
    # basis_vectors for core isis
    # Instances
    K = 3
    # DIMENSION
    N = 64
    A_VEC = N-2
    Q = 257
    R = 3
    Q2 = 257
    R2 = 5
    X = 0
    SEED_SIZE = 16
    if X == 0:
        allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        max_norm = 16
    sigma = 2

from libdntl import *


def gmp_ntt2(a, MODULUS=Q, ROOT_OF_UNITY=R):
    n = len(a)
    a = [gmpy2.mpz(x) for x in a]  # Convert input array elements to gmpy2.mpz
    # Precompute powers of the root of unity using gmpy2
    roots = [gmpy2.mpz(1)] * n
    root = gmpy2.powmod(ROOT_OF_UNITY, (MODULUS - 1) // n, MODULUS)
    for i in range(1, n):
        roots[i] = gmpy2.mul(roots[i - 1], root) % MODULUS
    # Bit-reversal permutation
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j >= bit:
            j -= bit
            bit >>= 1
        j += bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    # Iterative Cooley-Tukey NTT using gmpy2 for modular multiplication
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v + MODULUS) % MODULUS
        length *= 2
    return [int(x) for x in a]  # Convert back to Python integers if needed


def gmp_ntt_inverse2(a, MODULUS=Q, ROOT_OF_UNITY=R):
    n = len(a)
    a = [gmpy2.mpz(x) for x in a]  # Convert input array elements to gmpy2.mpz
    # Precompute the powers of the inverse root of unity using gmpy2
    root_inv = gmpy2.powmod(ROOT_OF_UNITY, MODULUS - 1 - (MODULUS - 1) // n, MODULUS)
    roots_inv = [gmpy2.mpz(1)] * n
    for i in range(1, n):
        roots_inv[i] = gmpy2.mul(roots_inv[i - 1], root_inv) % MODULUS
    # Bit-reversal permutation (same as forward NTT)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j >= bit:
            j -= bit
            bit >>= 1
        j += bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    # Iterative Cooley-Tukey NTT inverse using gmpy2 for modular multiplication
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots_inv[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v + MODULUS) % MODULUS
        length *= 2
    # Normalize with the inverse of n using gmpy2's modular inverse
    inv_n = gmpy2.invert(n, MODULUS)
    for i in range(n):
        a[i] = gmpy2.mul(a[i], inv_n) % MODULUS
    return [int(x) for x in a]  # Convert back to Python integers if needed


def gmp_ntt3(a, MODULUS=Q2, ROOT_OF_UNITY=R2):
    n = len(a)
    a = [gmpy2.mpz(x) for x in a]  # Convert input array elements to gmpy2.mpz
    # Precompute powers of the root of unity using gmpy2
    roots = [gmpy2.mpz(1)] * n
    root = gmpy2.powmod(ROOT_OF_UNITY, (MODULUS - 1) // n, MODULUS)
    for i in range(1, n):
        roots[i] = gmpy2.mul(roots[i - 1], root) % MODULUS
    # Bit-reversal permutation
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j >= bit:
            j -= bit
            bit >>= 1
        j += bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    # Iterative Cooley-Tukey NTT using gmpy2 for modular multiplication
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v + MODULUS) % MODULUS
        length *= 2
    return [int(x) for x in a]  # Convert back to Python integers if needed


def gmp_ntt_inverse3(a, MODULUS=Q2, ROOT_OF_UNITY=R2):
    n = len(a)
    a = [gmpy2.mpz(x) for x in a]  # Convert input array elements to gmpy2.mpz
    # Precompute the powers of the inverse root of unity using gmpy2
    root_inv = gmpy2.powmod(ROOT_OF_UNITY, MODULUS - 1 - (MODULUS - 1) // n, MODULUS)
    roots_inv = [gmpy2.mpz(1)] * n
    for i in range(1, n):
        roots_inv[i] = gmpy2.mul(roots_inv[i - 1], root_inv) % MODULUS
    # Bit-reversal permutation (same as forward NTT)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j >= bit:
            j -= bit
            bit >>= 1
        j += bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    # Iterative Cooley-Tukey NTT inverse using gmpy2 for modular multiplication
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots_inv[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v + MODULUS) % MODULUS
        length *= 2
    # Normalize with the inverse of n using gmpy2's modular inverse
    inv_n = gmpy2.invert(n, MODULUS)
    for i in range(n):
        a[i] = gmpy2.mul(a[i], inv_n) % MODULUS
    return [int(x) for x in a]  # Convert back to Python integers if needed


def sampleMatrixISISL1(seed, n, A_VEC=A_VEC):
    random.seed(seed)
    np.random.seed(seed % 2**32)
    A = []
    previous_accumulator = []
    CORE_MATRIX = []    
    for _ in range(n // 2): 
        pass_sampling = False
        while pass_sampling != True:
            while True:
                trial_vec1 = np.random.choice(CRange, size=n)
                ntt_rep1 = gmp_ntt2(trial_vec1)
                if not np.any((-X <= np.array(ntt_rep1)) & (np.array(ntt_rep1) <= X)):
                    break
            while True:
                trial_vec2 = np.random.choice(CRange, size=n)
                ntt_rep2 = gmp_ntt2(trial_vec2)
                if not np.any((-X <= np.array(ntt_rep2)) & (np.array(ntt_rep2) <= X)):
                    break  
            if len(previous_accumulator) < 1:
                ntt_accumulator = pointwise_multiplication(
                    previous_accumulator, ntt_rep2, Q
                )
                ntt_accumulator = pointwise_multiplication(ntt_accumulator, ntt_rep1, Q)
            else:
                ntt_accumulator = pointwise_multiplication(ntt_rep1, ntt_rep2, Q)
            # Accumulator checks
            if np.any(
                (-X <= np.array(ntt_accumulator)) & (np.array(ntt_accumulator) <= X)
            ):
                continue
            CORE_MATRIX.append(ntt_rep1)
            CORE_MATRIX.append(ntt_rep2)
            previous_accumulator = ntt_accumulator
            pass_sampling = True
    previous_accumulator = []
    A.append(CORE_MATRIX)

    for u in range(K - 1):
        sVP_MATRIX = []
        for _ in range(A_VEC // 2):
            pass_sampling = False
            previous_accumulator = []
            while pass_sampling != True:
                while True:
                    trial_vec1 = np.random.choice(CRange, size=n)
                    ntt_rep1 = gmp_ntt2(trial_vec1)
                    if not np.any((-X <= np.array(ntt_rep1)) & (np.array(ntt_rep1) <= X)):
                        break
                while True:
                    trial_vec2 = np.random.choice(CRange, size=n)
                    ntt_rep2 = gmp_ntt2(trial_vec2)
                    if not np.any((-X <= np.array(ntt_rep2)) & (np.array(ntt_rep2) <= X)):
                        break  
                if len(previous_accumulator) < 1:
                    ntt_accumulator = pointwise_multiplication(
                        previous_accumulator, ntt_rep2, Q
                    )
                    ntt_accumulator = pointwise_multiplication(
                        ntt_accumulator, ntt_rep1, Q
                    )
                else:
                    ntt_accumulator = pointwise_multiplication(ntt_rep1, ntt_rep2, Q)
                # Accumulator checks
                if np.any(
                    (-1 < np.array(ntt_accumulator)) & (np.array(ntt_accumulator) < 1)
                ):
                    continue
                else:
                    # If all checks are passed, add to the matrix
                    sVP_MATRIX.append(ntt_rep1)
                    sVP_MATRIX.append(ntt_rep2)
                    previous_accumulator = ntt_accumulator
                    pass_sampling = True
        A.append(sVP_MATRIX)
    return A


def verify(m, PK_C, pk, sig, u):
    SC = xof(u + m + pk.tobytes()).digest(SEED_SIZE)
    sig_seed = int.from_bytes(SC, byteorder="big")
    (S_A1) = sampleMatrixISISL1(sig_seed, N)
    pk_seed = int.from_bytes(PK_C, byteorder="big")
    (PK_A1) = sampleMatrixISISL1(pk_seed, N)
    lhs = pk
    rhs = sig
    for S1 in S_A1:
        for S_vec in S1:
            lhs = np.array(pointwise_multiplication(lhs, S_vec, Q))
        lhs = np.array(gmp_ntt_inverse2(lhs))
        lhs = np.array(gmp_ntt3(lhs))
        lhs_sq = np.array(pointwise_addition(lhs, lhs, Q2))
        lhs_sq = np.array(pointwise_addition(lhs_sq, lhs, Q2))
        lhs = np.array(gmp_ntt_inverse3(lhs_sq))
        lhs = np.array(gmp_ntt2(lhs))

    for PK in PK_A1:
        for PK_vec in PK:
            rhs = np.array(pointwise_multiplication(rhs, PK_vec, Q))
        rhs = np.array(gmp_ntt_inverse2(rhs))
        rhs = gmp_ntt3(rhs)
        rhs_SQ = np.array(pointwise_addition(rhs, rhs, Q2))
        rhs_SQ = np.array(pointwise_addition(rhs_SQ, rhs, Q2))
        rhs = np.array(gmp_ntt_inverse3(rhs_SQ))
        rhs = np.array(gmp_ntt2(rhs))
    if np.array_equal(lhs, rhs):
        print("\nPASS\n")
    else:
        print("\nFAIL\n")
        exit(1)


def sign(m, secret_x, PK_C, pk):
    sig = ()
    SIG_COMPLETED = False

    while SIG_COMPLETED == False:
        r1 = generate_random_bytes(SEED_SIZE)
        # u = Fiat-Heuristic to recreate the signature basis seed SC
        u = xof(r1 + PK_C + pk.tobytes()).digest(SEED_SIZE)
        # SC is bound to the pubic key and message
        SC = xof(u + m + pk.tobytes()).digest(SEED_SIZE)
        seed = int.from_bytes(SC, byteorder="big")
        # Each signature gets a public basis
        (S_A1) = sampleMatrixISISL1(seed, N)
        sig = secret_x  # direct injection - prevent norm explosion
        for S1 in S_A1: # One basis for each structure
            for S_vec in S1: #S_vec is already in the ntt2 domain
                sig = np.array(pointwise_multiplication(sig, S_vec, Q))
            sig = np.array(gmp_ntt_inverse2(sig)) # back to input domain
            sigsq = np.array(gmp_ntt3(sig)) # to transform domain
            sq = np.array(pointwise_addition(sigsq, sigsq, Q2)) # transform
            sq = np.array(pointwise_addition(sq, sigsq, Q2))
            sq = np.array(gmp_ntt_inverse3(sq)) #back to input domain
            sq = np.array(gmp_ntt2(sq)) # back to ntt2 domain
            sig = sq
        # Validate Zero Product Property
        if np.any((-1 < sig) & (sig < 1)):
            if trials == 5:
                trials = 0
            continue
        else:
            print("Message:", m.hex())
            print("Sig:", sig, "Sig Entropy", calculate_entropy(sig))
            return (sig, u)

def keyGen():
    SHORT_KEY = False
    while SHORT_KEY == False:
        secret_x = np.array(
            [
                gaussian_select_from_set( # Could be uniform
                    s=s, sigma=sigma, allowed_values=allowed_values
                )
                for s in range(N)
            ]
        )
        if calculate_norm(secret_x) <= max_norm:
            SHORT_KEY = True
    pk = ()
    PK_C = ""
    PK_COMPLETE = False
    trials = 0
    while PK_COMPLETE == False:
        trials += 1
        r1 = generate_random_bytes(SEED_SIZE)
        r2 = generate_random_bytes(SEED_SIZE)
        r3 = generate_random_bytes(SEED_SIZE)
        # Step 2: Hash r1 || r2 using SHAKE
        u = xof(r1 + r2).digest(SEED_SIZE)
        # Step 3: Hash u || r3 using SHAKE to get PK_C seed
        PK_C = xof(u + r3).digest(SEED_SIZE)
        seed = int.from_bytes(PK_C, byteorder="big") # numpy thing
        (PK_A1) = sampleMatrixISISL1(seed, N)
        pk = secret_x  # direct injection

        for PK in PK_A1:
            for PK_vec in PK: # PK_vec exists in ntt domain
                pk = np.array(pointwise_multiplication(pk, PK_vec, Q))
            pk = np.array(gmp_ntt_inverse2(pk)) # back to input domain
            pk = np.array(gmp_ntt3(pk)) # to transform domain
            pksq = np.array(pointwise_addition(pk, pk, Q2)) # transform
            pksq = np.array(pointwise_addition(pksq, pk, Q2)) 
            pksq = np.array(gmp_ntt_inverse3(pksq)) # invert to input domain
            pksq = np.array(gmp_ntt2(pksq)) # back to ntt2 domain
            pk = pksq
        # Ensure we maintain the zero product property
        if np.any((-1 < pk) & (pk < 1)):
            if trials == 5:
                SHORT_KEY = False
                while SHORT_KEY == False:
                    secret_x = np.array(
                        [
                            gaussian_select_from_set(
                                s=s, sigma=sigma, allowed_values=allowed_values
                            )
                            for s in range(N)
                        ]
                    )
                    if calculate_norm(secret_x) <= max_norm: #proper short
                        SHORT_KEY = True
                        trials = 0
            continue
        else:
            PK_COMPLETE = True
            print("PK:", pk, "\nPK Entropy:", calculate_entropy(pk))
            print(
                "SK:",
                secret_x,
                "\nSK Entropy:",
                calculate_entropy(secret_x),
                "L2:",
                calculate_norm(secret_x),
            )
            return (secret_x, pk, PK_C)

# Example usage
if __name__ == "__main__":
    for _ in range(10):
        (secret, pk, PK_C) = keyGen() #sk, pk, public basis seed
        m = generate_random_bytes(32)
        (SIG, u) = sign(m, secret, PK_C, pk) # signature, heuristic
        verify(m, PK_C, pk, SIG, u) # needs message, public seed, public key, sig seed and hueristic
        

