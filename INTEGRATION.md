# DNTL-DSA Integration Guide

## Overview

DNTL-DSA is a lattice-based digital signature algorithm based on k-DSP (k-chained Decision Short Problem) over natural lattices. This guide provides instructions for integrating DNTL-DSA into your applications.

**Warning:** This is a research implementation. Do not use in production systems without thorough security review.

## Table of Contents

- [System Requirements](#system-requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Security Levels](#security-levels)
- [Integration Examples](#integration-examples)
- [Serialization](#serialization)
- [Performance Considerations](#performance-considerations)
- [Security Considerations](#security-considerations)
- [Troubleshooting](#troubleshooting)

## System Requirements

### Python Implementation

- **Python:** 3.7 or higher
- **Dependencies:**
  - `numpy` - Array operations and linear algebra
  - `gmpy2` - High-performance modular arithmetic
  - `hashlib` (standard library) - Cryptographic hashing

### Optional C Extensions (for performance)

- **Compiler:** GCC 7+ or Clang 9+
- **Libraries:**
  - libbrotli (for compression)
  - SIMD support (AVX2 for x86, NEON for ARM)

## Installation

### Basic Installation

1. Clone the repository:
```bash
git clone https://github.com/yourusername/dntl.git
cd dntl
```

2. Install Python dependencies:
```bash
pip install -r requirements.txt
```

3. Verify installation:
```bash
python3 dntl-dsa-nat.py -c 1
```

### Installing as a Python Module

To use DNTL-DSA as an importable module in your project:

```bash
# Add dntl directory to your PYTHONPATH
export PYTHONPATH="${PYTHONPATH}:/path/to/dntl"
```

Or copy the required files to your project:
```bash
cp dntl-dsa-nat.py your_project/
cp libdntln.py your_project/
```

## Quick Start

### Basic Usage

```python
#!/usr/bin/env python3
import numpy as np
from dntl-dsa-nat import keyGen, sign, verify

# Generate a key pair (Level 1 security)
secret_key, public_key, pk_seed = keyGen()

# Sign a message
message = b"Hello, World!"
signature, nonce = sign(message, secret_key, pk_seed, public_key)

# Verify the signature
verify(message, pk_seed, public_key, signature, nonce)
# Prints "PASS" if valid, "FAIL" otherwise
```

### Selecting Security Level

Configure security level before importing:

```python
import sys
import argparse

# Level 1: 80 bytes (N=64, K=2)
# Level 3: 152 bytes (N=128, K=2)
# Level 5: 288 bytes (N=256, K=3)

sys.argv = ['program', '-c', '3']  # Use Level 3
import dntl-dsa-nat as dntl
```

## API Reference

### Key Generation

```python
keyGen() -> (secret_key, public_key, pk_seed)
```

Generates a fresh key pair.

**Returns:**
- `secret_key` (numpy.ndarray): Secret signing key with Gaussian-sampled coefficients
- `public_key` (numpy.ndarray): Public verification key
- `pk_seed` (bytes): Public seed for key derivation (SEED_SIZE bytes)

**Properties:**
- Secret key has L2 norm ≤ `max_norm` (24 for Level 1, 48 for Level 3, 100 for Level 5)
- Public key is deterministically derived from secret key and pk_seed
- Keys are in NTT domain for efficient operations

**Example:**
```python
sk, pk, pk_seed = keyGen()
print(f"Public key size: {len(pk)} elements")
print(f"Public key entropy: {calculate_entropy(pk)}")
```

### Signing

```python
sign(message, secret_key, pk_seed, public_key) -> (signature, nonce)
```

Signs a message using the secret key.

**Parameters:**
- `message` (bytes): Message to sign (typically 32 bytes from hash output)
- `secret_key` (numpy.ndarray): Secret key from keyGen()
- `pk_seed` (bytes): Public key seed from keyGen()
- `public_key` (numpy.ndarray): Public key from keyGen()

**Returns:**
- `signature` (numpy.ndarray): Signature vector in NTT domain
- `nonce` (bytes): Fiat-Shamir nonce for signature basis (SEED_SIZE bytes)

**Notes:**
- Uses SHAKE-256 XOF for deterministic signature basis generation
- Signature is rejection-sampled until valid (no modular overflow)
- Message should be pre-hashed for variable-length inputs

**Example:**
```python
import hashlib

# Hash long messages before signing
msg_hash = hashlib.sha256(long_message).digest()
sig, u = sign(msg_hash, sk, pk_seed, pk)
```

### Verification

```python
verify(message, pk_seed, public_key, signature, nonce) -> None
```

Verifies a signature against a message and public key.

**Parameters:**
- `message` (bytes): Original message that was signed
- `pk_seed` (bytes): Public key seed
- `public_key` (numpy.ndarray): Public key
- `signature` (numpy.ndarray): Signature to verify
- `nonce` (bytes): Signature nonce

**Returns:**
- None (prints "PASS" or "FAIL" and exits on failure)

**Example:**
```python
verify(msg_hash, pk_seed, pk, sig, u)
```

### Utility Functions (libdntln.py)

```python
# Generate cryptographic random bytes
generate_random_bytes(length: int) -> bytes

# Calculate L2 norm of vector
calculate_norm(vector: np.ndarray) -> int

# Calculate Shannon entropy
calculate_entropy(array: np.ndarray) -> float

# Pointwise multiplication in Fq
pointwise_multiplication(vec1, vec2, modulus=257) -> list

# Pointwise addition in Fq
pointwise_addition(vec1, vec2, modulus=257) -> list
```

## Security Levels

| Level | N (dim) | K (instances) | Key Size | Signature Size | Security Estimate |
|-------|---------|---------------|----------|----------------|-------------------|
| 1     | 64      | 2             | ~80 B    | ~80 B          | Research level    |
| 3     | 128     | 2             | ~152 B   | ~152 B         | Research level    |
| 5     | 256     | 3             | ~288 B   | ~288 B         | Research level    |

**Parameters:**
- `Q = 257` (prime modulus for natural numbers [1, 257])
- `R = 3` (root of unity for NTT)
- `Q2 = 257`, `R2 = 5` (transform domain parameters)
- `allowed_values = [1,2,3,4,5]` (secret key alphabet)

**Note:** Size estimates assume dense encoding. See [Serialization](#serialization) for compression options.

## Integration Examples

### Example 1: Signing User Transactions

```python
import hashlib
from dntl_dsa_nat import keyGen, sign, verify

class TransactionSigner:
    def __init__(self, security_level=1):
        self.sk, self.pk, self.pk_seed = keyGen()

    def sign_transaction(self, transaction_data):
        """Sign arbitrary transaction data"""
        # Hash the transaction
        tx_hash = hashlib.sha256(transaction_data.encode()).digest()

        # Sign the hash
        sig, nonce = sign(tx_hash, self.sk, self.pk_seed, self.pk)

        return {
            'signature': sig.tobytes(),
            'nonce': nonce,
            'public_key': self.pk.tobytes(),
            'pk_seed': self.pk_seed
        }

    @staticmethod
    def verify_transaction(transaction_data, sig_data):
        """Verify a signed transaction"""
        tx_hash = hashlib.sha256(transaction_data.encode()).digest()

        # Reconstruct numpy arrays
        pk = np.frombuffer(sig_data['public_key'], dtype=int)
        sig = np.frombuffer(sig_data['signature'], dtype=int)

        verify(tx_hash, sig_data['pk_seed'], pk, sig, sig_data['nonce'])
        return True  # Returns only if verification passes

# Usage
signer = TransactionSigner(security_level=1)
tx = "Alice pays Bob 100 coins"
signed_tx = signer.sign_transaction(tx)
TransactionSigner.verify_transaction(tx, signed_tx)
```

### Example 2: Compressed Signatures with Sparse Encoding

```python
from sparse_encoding import SparseVectorEncoder
from dntl_dsa_nat import keyGen, sign, verify

# Initialize encoder
encoder = SparseVectorEncoder(dimension=64, value_bits=16)

# Generate keys
sk, pk, pk_seed = keyGen()

# Sign message
message = b"Compressed signature example"
sig, nonce = sign(message, sk, pk_seed, pk)

# Compress signature (if sparse)
sig_compressed = encoder.encode_coo(sig)
print(f"Original size: {sig.nbytes} bytes")
print(f"Compressed: {len(sig_compressed)} bytes")

# For verification, decompress first
sig_decompressed = encoder.decode_coo(sig_compressed)
verify(message, pk_seed, pk, sig_decompressed, nonce)
```

### Example 3: Multi-Message Signing

```python
def sign_batch(messages, secret_key, pk_seed, public_key):
    """Sign multiple messages efficiently"""
    signatures = []

    for msg in messages:
        # Pre-hash each message
        msg_hash = hashlib.sha256(msg).digest()
        sig, nonce = sign(msg_hash, secret_key, pk_seed, public_key)
        signatures.append((sig, nonce))

    return signatures

def verify_batch(messages, public_key, pk_seed, signatures):
    """Verify multiple signatures"""
    results = []

    for msg, (sig, nonce) in zip(messages, signatures):
        msg_hash = hashlib.sha256(msg).digest()
        try:
            verify(msg_hash, pk_seed, public_key, sig, nonce)
            results.append(True)
        except SystemExit:
            results.append(False)

    return results

# Usage
messages = [b"msg1", b"msg2", b"msg3"]
sk, pk, pk_seed = keyGen()
sigs = sign_batch(messages, sk, pk_seed, pk)
results = verify_batch(messages, pk, pk_seed, sigs)
```

### Example 4: Key Serialization and Storage

```python
import pickle
import json
import base64

class KeyManager:
    @staticmethod
    def serialize_keypair(sk, pk, pk_seed):
        """Serialize keys for storage"""
        return {
            'secret_key': base64.b64encode(sk.tobytes()).decode('utf-8'),
            'public_key': base64.b64encode(pk.tobytes()).decode('utf-8'),
            'pk_seed': base64.b64encode(pk_seed).decode('utf-8'),
            'dimension': len(sk)
        }

    @staticmethod
    def deserialize_keypair(data):
        """Restore keys from serialized format"""
        sk = np.frombuffer(
            base64.b64decode(data['secret_key']),
            dtype=np.int64
        )[:data['dimension']]

        pk = np.frombuffer(
            base64.b64decode(data['public_key']),
            dtype=np.int64
        )[:data['dimension']]

        pk_seed = base64.b64decode(data['pk_seed'])

        return sk, pk, pk_seed

    @staticmethod
    def save_to_file(filepath, sk, pk, pk_seed):
        """Save keys to JSON file"""
        data = KeyManager.serialize_keypair(sk, pk, pk_seed)
        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)

    @staticmethod
    def load_from_file(filepath):
        """Load keys from JSON file"""
        with open(filepath, 'r') as f:
            data = json.load(f)
        return KeyManager.deserialize_keypair(data)

# Usage
sk, pk, pk_seed = keyGen()
KeyManager.save_to_file('keys.json', sk, pk, pk_seed)
sk2, pk2, pk_seed2 = KeyManager.load_from_file('keys.json')
```

## Serialization

### Standard Format (Dense)

```python
# Public key serialization
pk_bytes = public_key.tobytes()  # N * 8 bytes (int64)

# Signature serialization
sig_bytes = signature.tobytes()  # N * 8 bytes (int64)

# Full signature package
sig_package = {
    'signature': sig_bytes,
    'nonce': nonce,  # SEED_SIZE bytes
    'pk_seed': pk_seed  # SEED_SIZE bytes
}
```

### Compressed Format (Sparse Encoding)

For signatures with sparse structure, use the sparse encoding module:

```python
from sparse_encoding import SparseVectorEncoder

# Configure encoder based on security level
encoder = SparseVectorEncoder(dimension=N, value_bits=16)

# Compress signature (if sparse)
sig_compressed = encoder.encode_coo(signature)

# Typical sizes with compression:
# Level 1 (N=64):  ~50 bytes  (vs 512 bytes dense)
# Level 3 (N=128): ~100 bytes (vs 1024 bytes dense)
# Level 5 (N=256): ~200 bytes (vs 2048 bytes dense)
```

See [SPARSE_ENCODING_README.md](SPARSE_ENCODING_README.md) for detailed compression options.

### Wire Format Recommendation

```python
import struct

def serialize_signature(sig, nonce, pk_seed):
    """Create compact wire format"""
    # Header: version (1B) + format (1B) + dimension (2B)
    header = struct.pack('BBHH', 1, 0, len(sig), 0)

    # Payload
    payload = sig.tobytes() + nonce + pk_seed

    return header + payload

def deserialize_signature(data):
    """Parse wire format"""
    version, fmt, dimension, _ = struct.unpack('BBHH', data[:6])

    offset = 6
    sig = np.frombuffer(data[offset:offset+dimension*8], dtype=np.int64)
    offset += dimension * 8

    nonce = data[offset:offset+SEED_SIZE]
    offset += SEED_SIZE

    pk_seed = data[offset:offset+SEED_SIZE]

    return sig, nonce, pk_seed
```

## Performance Considerations

### Timing (Python Implementation)

On a modern CPU (single core):

| Operation | Level 1 (N=64) | Level 3 (N=128) | Level 5 (N=256) |
|-----------|----------------|-----------------|-----------------|
| KeyGen    | ~50-100 ms     | ~100-200 ms     | ~200-400 ms     |
| Sign      | ~30-60 ms      | ~60-120 ms      | ~120-240 ms     |
| Verify    | ~20-40 ms      | ~40-80 ms       | ~80-160 ms      |

**Note:** Times include rejection sampling overhead. Actual times vary based on sampling luck.

### Optimization Options

1. **C Implementation**: Use compiled C code for 10-100x speedup
   - See `ntt64.c` for optimized NTT operations
   - See `sparse_*.c` for compression implementations

2. **SIMD Acceleration**: Enable AVX2/NEON for NTT operations
   ```bash
   make -f Makefile.simd
   ```
   See [SIMD_README.md](SIMD_README.md) for details.

3. **Precomputation**: Cache NTT twiddle factors
   ```python
   # Precompute roots of unity once
   roots = precompute_ntt_roots(N, Q, R)
   ```

4. **Batch Verification**: Verify multiple signatures together
   - Amortize basis generation cost
   - ~30% faster for 10+ signatures

### Memory Usage

| Component | Level 1 | Level 3 | Level 5 |
|-----------|---------|---------|---------|
| Secret Key | 512 B | 1 KB | 2 KB |
| Public Key | 512 B | 1 KB | 2 KB |
| Signature | 512 B | 1 KB | 2 KB |
| Working Memory | ~10 KB | ~20 KB | ~40 KB |

With sparse encoding, storage requirements drop by ~5-10x.

## Security Considerations

### Current Status

⚠️ **Warning**: This is a research implementation. Key considerations:

1. **Academic Review**: The core k-DSP hardness assumption requires peer review
2. **Constant-Time**: Python implementation is NOT constant-time (vulnerable to timing attacks)
3. **Side Channels**: No protections against cache-timing or power analysis
4. **Randomness**: Relies on `os.urandom()` - ensure proper entropy source

### Known Limitations

- Secret keys are small Gaussian-sampled vectors (potential lattice attacks)
- Non-core instances are slightly rank-deficient (see README.md)
- NTT domain operations may leak information through timing
- No formal security proof published

### Recommendations for Production Use

1. **Do NOT use without:**
   - Full security audit by lattice cryptography experts
   - Constant-time C implementation (see `CONSTANT_TIME_ANALYSIS.md`)
   - Side-channel protections
   - Formal security analysis

2. **Use cases appropriate for research:**
   - Academic experiments
   - Cryptographic protocol research
   - Performance benchmarking
   - Algorithm development

3. **For production systems:**
   - Use standardized schemes (NIST PQC finalists: CRYSTALS-Dilithium, Falcon, SPHINCS+)
   - Wait for peer review and cryptanalysis
   - Implement in constant-time languages (C, Rust)

### Safe Integration Practices

```python
# DO: Hash messages before signing
msg_hash = hashlib.sha256(message).digest()
sig, nonce = sign(msg_hash, sk, pk_seed, pk)

# DON'T: Sign raw user input
sig, nonce = sign(user_input, sk, pk_seed, pk)  # Dangerous!

# DO: Validate all inputs
if len(message) != 32:
    raise ValueError("Message must be 32-byte hash")

# DO: Protect secret keys
import os
sk_secure = np.array(sk)
# ... use sk_secure ...
sk_secure.fill(0)  # Zeroize after use
del sk_secure
```

## Troubleshooting

### Common Issues

**1. Import errors**
```
ModuleNotFoundError: No module named 'gmpy2'
```
Solution:
```bash
pip install gmpy2
# On Ubuntu/Debian:
sudo apt-get install libgmp-dev libmpfr-dev libmpc-dev
pip install gmpy2
```

**2. NTT domain errors**
```
if np.any(257 <= sig):
```
Solution: This is normal - the algorithm rejection-samples until finding a valid signature. If it loops indefinitely, check your secret key norm.

**3. Slow key generation**
```
KeyGen taking minutes...
```
Solution: The algorithm samples until finding a short secret. For Level 5, this can take multiple attempts. Consider using Level 1 or 3 for testing.

**4. Verification always fails**
```
FAIL
```
Solution: Ensure you're using the same message, public key, and pk_seed for both signing and verification. Check serialization/deserialization is preserving array values.

### Debug Mode

Enable verbose output:

```python
import os
os.environ['DNTL_DEBUG'] = '1'

# Add to dntl-dsa-nat.py:
DEBUG = os.getenv('DNTL_DEBUG', '0') == '1'

if DEBUG:
    print(f"Secret norm: {calculate_norm(secret_x)}")
    print(f"PK values: {pk}")
```

## Additional Resources

- **[README.md](README.md)**: Project overview and high-level description
- **[SPARSE_ENCODING_README.md](SPARSE_ENCODING_README.md)**: Compression techniques
- **[SIMD_README.md](SIMD_README.md)**: SIMD optimization guide
- **[CONSTANT_TIME_ANALYSIS.md](CONSTANT_TIME_ANALYSIS.md)**: Side-channel considerations

## Support

For questions or issues:
- **Email**: info@quantumshield.us
- **Issues**: File issues on the repository
- **Research**: Check recent papers on k-DSP and natural lattices

## License

See [LICENCE.txt](LICENCE.txt) for details.

---

**Last Updated**: 2025-11-22
