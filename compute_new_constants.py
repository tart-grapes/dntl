#!/usr/bin/env python3
"""Compute NTT constants for new prime layers"""

import sys

def mod_exp(base, exp, mod):
    """Modular exponentiation"""
    result = 1
    base = base % mod
    while exp > 0:
        if exp % 2 == 1:
            result = (result * base) % mod
        exp = exp >> 1
        base = (base * base) % mod
    return result

def mod_inv(a, m):
    """Extended Euclidean Algorithm for modular inverse"""
    def extended_gcd(a, b):
        if a == 0:
            return b, 0, 1
        gcd, x1, y1 = extended_gcd(b % a, a)
        x = y1 - (b // a) * x1
        y = x1
        return gcd, x, y

    gcd, x, _ = extended_gcd(a % m, m)
    if gcd != 1:
        return None
    return (x % m + m) % m

def find_primitive_root(q):
    """Find a primitive root modulo prime q"""
    # Factor q-1
    qm1 = q - 1
    factors = []
    temp = qm1
    for p in [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31]:
        while temp % p == 0:
            if not factors or factors[-1] != p:
                factors.append(p)
            temp //= p
    if temp > 1:
        factors.append(temp)

    # Try candidate primitive roots
    for g in range(2, q):
        is_primitive = True
        for p in factors:
            if mod_exp(g, qm1 // p, q) == 1:
                is_primitive = False
                break
        if is_primitive:
            return g
    return None

def tonelli_shanks(n, p):
    """Find square root of n modulo p using Tonelli-Shanks"""
    if mod_exp(n, (p - 1) // 2, p) != 1:
        return None

    # Factor out powers of 2 from p-1
    q = p - 1
    s = 0
    while q % 2 == 0:
        q //= 2
        s += 1

    # Find a quadratic non-residue
    z = 2
    while mod_exp(z, (p - 1) // 2, p) != p - 1:
        z += 1

    m = s
    c = mod_exp(z, q, p)
    t = mod_exp(n, q, p)
    r = mod_exp(n, (q + 1) // 2, p)

    while True:
        if t == 0:
            return 0
        if t == 1:
            return r

        # Find least i such that t^(2^i) = 1
        i = 1
        temp = (t * t) % p
        while temp != 1:
            temp = (temp * temp) % p
            i += 1

        b = mod_exp(c, 1 << (m - i - 1), p)
        m = i
        c = (b * b) % p
        t = (t * c) % p
        r = (r * b) % p

def compute_ntt_constants(q, N=64):
    """Compute all NTT constants for prime q"""
    print(f"\n{'='*60}")
    print(f"Computing constants for q = {q:,}")
    print(f"{'='*60}")

    # Check q ≡ 1 (mod 2N)
    if (q - 1) % (2 * N) != 0:
        print(f"ERROR: q-1 must be divisible by {2*N}")
        return None

    # Find primitive root
    g = find_primitive_root(q)
    if g is None:
        print("ERROR: Could not find primitive root")
        return None
    print(f"Primitive root g = {g}")

    # For negacyclic NTT, we need psi where:
    # - psi^(2N) = 1 (psi is a primitive 2N-th root of unity)
    # - psi^N = -1 (negacyclic condition)
    # Then omega = psi^2 satisfies omega^N = 1

    # Compute psi (primitive 2N-th root with psi^N = -1)
    # This is g^((q-1)/(2N)) where g is a primitive root
    psi = mod_exp(g, (q - 1) // (2 * N), q)
    print(f"psi (2N-th root) = {psi}")

    # Verify psi^N = -1 and psi^(2N) = 1
    psi_n = mod_exp(psi, N, q)
    psi_2n = mod_exp(psi, 2 * N, q)
    if psi_n != q - 1:
        print(f"ERROR: psi^N = {psi_n} != -1 = {q-1}")
        return None
    if psi_2n != 1:
        print(f"ERROR: psi^(2N) = {psi_2n} != 1")
        return None

    # Compute omega = psi^2
    omega = (psi * psi) % q
    print(f"omega (psi^2) = {omega}")

    # Compute N_inv
    n_inv = mod_inv(N, q)
    print(f"N_inv (64^-1 mod q) = {n_inv}")

    # Compute Barrett constant
    barrett = (1 << 64) // q
    print(f"Barrett constant = {barrett}")

    # Compute twiddle factors for forward NTT
    twiddles_fwd = []
    for stage in range(6):  # log2(64) = 6 stages
        twiddle = mod_exp(omega, N // (1 << (stage + 1)), q)
        twiddles_fwd.append(twiddle)
    print(f"Twiddles FWD = {twiddles_fwd}")

    # Compute twiddle factors for inverse NTT
    omega_inv = mod_inv(omega, q)
    twiddles_inv = []
    for stage in range(6):
        twiddle = mod_exp(omega_inv, N // (1 << (stage + 1)), q)
        twiddles_inv.append(twiddle)
    print(f"Twiddles INV = {twiddles_inv}")

    # Compute psi powers
    psi_powers = []
    for i in range(N):
        psi_powers.append(mod_exp(psi, i, q))

    # Compute psi inverse powers
    psi_inv = mod_inv(psi, q)
    psi_inv_powers = []
    for i in range(N):
        psi_inv_powers.append(mod_exp(psi_inv, i, q))

    return {
        'q': q,
        'g': g,
        'omega': omega,
        'psi': psi,
        'n_inv': n_inv,
        'barrett': barrett,
        'twiddles_fwd': twiddles_fwd,
        'twiddles_inv': twiddles_inv,
        'psi_powers': psi_powers,
        'psi_inv_powers': psi_inv_powers
    }

# New primes to compute
new_primes = [
    43777,        # Layer 3 replacement
    686593,       # Layer 5 replacement
    2818573313    # Layer 6 replacement
]

all_constants = {}
for q in new_primes:
    constants = compute_ntt_constants(q)
    if constants:
        all_constants[q] = constants

# Print C code
print("\n" + "="*60)
print("C CODE FOR NEW CONSTANTS")
print("="*60)

for q in new_primes:
    if q not in all_constants:
        continue
    c = all_constants[q]

    print(f"\n// Layer for q = {q:,}")
    print(f"{q}u,")
    print(f"N_INV: {c['n_inv']}u,")
    print(f"BARRETT: {c['barrett']}ULL,")

    print(f"TWIDDLES_FWD: {{", end="")
    print(", ".join(f"{x}u" for x in c['twiddles_fwd']), end="")
    print(" },")

    print(f"TWIDDLES_INV: {{", end="")
    print(", ".join(f"{x}u" for x in c['twiddles_inv']), end="")
    print(" },")

    print(f"PSI_POWERS (first 16): {{", end="")
    print(", ".join(f"{x}u" for x in c['psi_powers'][:16]), end="")
    print(", ...},")

    print(f"PSI_INV_POWERS (first 16): {{", end="")
    print(", ".join(f"{x}u" for x in c['psi_inv_powers'][:16]), end="")
    print(", ...}")
