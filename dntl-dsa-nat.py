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
#CRange = np.concatenate((np.arange(-127, 0, dtype=int), np.arange(1, 128, dtype=int)))
CRange = np.arange(1,258, dtype=int).tolist()
print(CRange)
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
        #allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        allowed_values = [1,2,3,4,5]

        max_norm = 22.627416998
        max_norm = 48
    sigma = .5

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
        #allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        allowed_values = [1,2,3,4,5]

        max_norm = 30
    sigma = .5


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
        #allowed_values = [-6, -5, -4, -3, -2, -1,1,2, 3, 4, 5, 6]
        allowed_values = [1,2,3,4,5]
        max_norm = 30
    sigma = .8

from libdntln import *

def natural_mod(x, MODULUS):
    # Custom natural modulo function that maps to [1, MODULUS]
    res = x % MODULUS
    return res if res != 0 else MODULUS

def pre_ntt_shift(x, MODULUS):
    # Shift values from [1, q] to [0, q-1]
    return (x - 1) % MODULUS

def post_ntt_shift(x, MODULUS):
    # Shift values from [0, q-1] back to [1, q]
    return (x + 1) % MODULUS if x != 0 else MODULUS


import gmpy2

def forward_ntt_naturals(a, MODULUS=257, ROOT_OF_UNITY=3):
    n = len(a)
    
    # Convert input array elements to gmpy2.mpz
    a = [gmpy2.mpz(x) for x in a]
    
    # Precompute powers of the root of unity
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
    
    # Iterative Cooley-Tukey NTT using modular multiplication
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v) % MODULUS
                # Ensure all values are in the range [1, MODULUS]
                if a[i + j + half_len] == 0:
                    a[i + j + half_len] = MODULUS
                if a[i + j] == 0:
                    a[i + j] = MODULUS
        length *= 2

    return [int(x) for x in a]  # Convert back to Python integers if needed

def inverse_ntt_naturals(a, MODULUS=257, ROOT_OF_UNITY=3):
    n = len(a)
    
    # Convert input array elements to gmpy2.mpz
    a = [gmpy2.mpz(x) for x in a]
    
    # Precompute the powers of the inverse root of unity
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
    
    # Iterative Cooley-Tukey NTT inverse
    length = 2
    while length <= n:
        half_len = length // 2
        for i in range(0, n, length):
            for j in range(half_len):
                u = a[i + j]
                v = gmpy2.mul(a[i + j + half_len], roots_inv[n // length * j]) % MODULUS
                a[i + j] = (u + v) % MODULUS
                a[i + j + half_len] = (u - v) % MODULUS
                # Ensure all values are in the range [1, MODULUS]
                if a[i + j + half_len] == 0:
                    a[i + j + half_len] = MODULUS
                if a[i + j] == 0:
                    a[i + j] = MODULUS
        length *= 2
    
    # Normalize with the inverse of n using gmpy2's modular inverse
    inv_n = gmpy2.invert(n, MODULUS)
    for i in range(n):
        a[i] = gmpy2.mul(a[i], inv_n) % MODULUS
        if a[i] == 0:
            a[i] = MODULUS

    return [int(x) for x in a]  # Convert back to Python integers if needed

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

def filter_basis(basis, modulus=257):
    # Replace 257 (additive identity) with a safe value, like 1
    return [x if x != modulus else 1 for x in basis]

def sampleMatrixISISL2(seed, n, A_VEC=A_VEC):
    random.seed(seed)
    np.random.seed(seed % 2**32)
    A = []
    previous_accumulator = []
    CORE_MATRIX = []    
    for _ in range(n // 2):
        while True:
            trial_vec1 = filter_basis(np.random.choice(CRange, size=n))
            ntt_rep1 = forward_ntt_naturals(trial_vec1)
            if not np.any(257 <= np.array(ntt_rep1)):
                break

        while True:
            trial_vec2 = filter_basis(np.random.choice(CRange, size=n))
            ntt_rep2 = forward_ntt_naturals(trial_vec2)
            if not np.any(257 <= np.array(ntt_rep2)):
                break

        CORE_MATRIX.append(ntt_rep1)
        CORE_MATRIX.append(ntt_rep2)
    A.append(CORE_MATRIX)

    for u in range(K - 1):
        sVP_MATRIX = []
        for _ in range(A_VEC // 2):
            while True:
                trial_vec1 = filter_basis(np.random.choice(CRange, size=n))
                ntt_rep1 = forward_ntt_naturals(trial_vec1)
                if not np.any(257 <= np.array(ntt_rep1)):
                    break
            while True:
                trial_vec2 = filter_basis(np.random.choice(CRange, size=n))
                ntt_rep2 = forward_ntt_naturals(trial_vec2)
                if not np.any(257 <= np.array(ntt_rep2)):
                    break
            sVP_MATRIX.append(ntt_rep1)
            sVP_MATRIX.append(ntt_rep2)


        A.append(sVP_MATRIX)
    
    return A

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
    (S_A1) = sampleMatrixISISL2(sig_seed, N)
    pk_seed = int.from_bytes(PK_C, byteorder="big")
    (PK_A1) = sampleMatrixISISL2(pk_seed, N)
    lhs = pk
    rhs = sig
    for S1 in S_A1:
        for S_vec in S1:
            lhs = np.array(pointwise_multiplication(lhs, S_vec, Q))
        lhs = np.array(inverse_ntt_naturals(lhs))
        lhs = np.array(forward_ntt_naturals(lhs, Q2, R2))
        lhs_sq = np.array(pointwise_addition(lhs, lhs, Q2))
        lhs_sq = np.array(pointwise_addition(lhs_sq, lhs, Q2))
        lhs = np.array(inverse_ntt_naturals(lhs_sq, Q2, R2))
        lhs = np.array(forward_ntt_naturals(lhs))

    for PK in PK_A1:
        for PK_vec in PK:
            rhs = np.array(pointwise_multiplication(rhs, PK_vec, Q))
        rhs = np.array(inverse_ntt_naturals(rhs))
        rhs = forward_ntt_naturals(rhs, Q2, R2)
        rhs_SQ = np.array(pointwise_addition(rhs, rhs, Q2))
        rhs_SQ = np.array(pointwise_addition(rhs_SQ, rhs, Q2))

        rhs = np.array(inverse_ntt_naturals(rhs_SQ, Q2, R2))
        rhs = np.array(forward_ntt_naturals(rhs))
    print("l",lhs)
    print("r", rhs)
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
        (S_A1) = sampleMatrixISISL2(seed, N)
        sig = secret_x  # direct injection - prevent norm explosion
        for S1 in S_A1: # One basis for each structure
            for S_vec in S1: #S_vec is already in the ntt2 domain
                sig = np.array(pointwise_multiplication(sig, S_vec, Q))
            sig = np.array(inverse_ntt_naturals(sig)) # back to input domain
            sigsq = np.array(forward_ntt_naturals(sig, Q2, R2)) # to transform domain
            sq = np.array(pointwise_addition(sigsq, sigsq, Q2)) # transform
            sq = np.array(pointwise_addition(sq, sigsq, Q2))
            sq = np.array(inverse_ntt_naturals(sq,Q2,R2)) #back to input domain
            sq = np.array(forward_ntt_naturals(sq)) # back to ntt2 domain
            sig = sq
        # Validate Zero Product Property
        if np.any((257 <= sig)):

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
        (PK_A1) = sampleMatrixISISL2(seed, N)
        pk = secret_x  # direct injection

        for PK in PK_A1:
            for PK_vec in PK: # PK_vec exists in ntt domain
                pk = np.array(pointwise_multiplication(pk, PK_vec, Q))
                #print("pk",pk)
            pk = np.array(inverse_ntt_naturals(pk)) # back to input domain
            pk = np.array(forward_ntt_naturals(pk, Q2,R2)) # to transform domain
            pksq = np.array(pointwise_addition(pk, pk, Q2)) # transform
            pksq = np.array(pointwise_addition(pksq, pk, Q2)) 
            pksq = np.array(inverse_ntt_naturals(pksq, Q2,R2)) # invert to input domain
            pksq = np.array(forward_ntt_naturals(pksq)) # back to ntt2 domain
            pk = pksq
        # Ensure we maintain the zero product property
        if np.any((257 <= pk)):
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
                print([int(x) - 3 for x in secret_x ]),
                print(calculate_norm([int(x) - 3 for x in secret_x ])),

            )
            return (secret_x, pk, PK_C)

# Example usage
if __name__ == "__main__":
    for _ in range(100):
        (secret, pk, PK_C) = keyGen() #sk, pk, public basis seed
        m = generate_random_bytes(32)
        (SIG, u) = sign(m, secret, PK_C, pk) # signature, heuristic
        verify(m, PK_C, pk, SIG, u) # needs message, public seed, public key, sig seed and hueristic
        

