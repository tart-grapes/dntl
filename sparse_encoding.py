#!/usr/bin/env python3
"""
Sparse Vector Encoding for DNTL
Optimized for vectors with L2 norm constraints in high dimensions.

Supports multiple encoding formats:
- COO (Coordinate List): Best for arbitrary sparsity
- RLE (Run-Length): Best for clustered non-zeros
- Packed Binary: Best for small value ranges
"""

import struct
import numpy as np
from typing import List, Tuple, Optional
from math import sqrt


class SparseVectorEncoder:
    """
    Encodes sparse vectors efficiently for cryptographic applications.
    Designed for vectors with known L2 norms in high-dimensional spaces.
    """

    def __init__(self, dimension: int = 2048, value_bits: int = 8):
        """
        Args:
            dimension: Vector dimension (max 65535)
            value_bits: Bits per value (8, 16, or 32)
        """
        self.dimension = dimension
        self.value_bits = value_bits

        if dimension > 65535:
            raise ValueError("Dimension must be <= 65535 for 16-bit indices")
        if value_bits not in [8, 16, 32]:
            raise ValueError("value_bits must be 8, 16, or 32")

    def encode_coo(self, vector: np.ndarray, threshold: float = 1e-10) -> bytes:
        """
        Encode using Coordinate List (COO) format.
        Format: [count:2] [index:2, value:N]*

        Args:
            vector: Input vector (1D numpy array)
            threshold: Values below this are considered zero

        Returns:
            Encoded bytes
        """
        # Find non-zero entries
        nonzeros = [(i, v) for i, v in enumerate(vector) if abs(v) > threshold]
        count = len(nonzeros)

        if count > 65535:
            raise ValueError(f"Too many non-zeros: {count} (max 65535)")

        # Pack header: count
        data = bytearray(struct.pack('>H', count))

        # Pack each (index, value) pair
        if self.value_bits == 8:
            # 8-bit signed values [-128, 127]
            for idx, val in nonzeros:
                data.extend(struct.pack('>Hb', idx, int(val)))
        elif self.value_bits == 16:
            # 16-bit signed values [-32768, 32767]
            for idx, val in nonzeros:
                data.extend(struct.pack('>Hh', idx, int(val)))
        else:  # 32-bit
            # 32-bit float
            for idx, val in nonzeros:
                data.extend(struct.pack('>Hf', idx, float(val)))

        return bytes(data)

    def decode_coo(self, data: bytes) -> np.ndarray:
        """
        Decode COO format back to dense vector.

        Args:
            data: Encoded bytes

        Returns:
            Dense numpy array
        """
        # Initialize zero vector
        vector = np.zeros(self.dimension, dtype=np.float64)

        # Read count
        count = struct.unpack('>H', data[:2])[0]

        # Read each (index, value) pair
        pos = 2
        if self.value_bits == 8:
            entry_size = 3  # 2 bytes index + 1 byte value
            for _ in range(count):
                idx, val = struct.unpack('>Hb', data[pos:pos+entry_size])
                vector[idx] = val
                pos += entry_size
        elif self.value_bits == 16:
            entry_size = 4  # 2 bytes index + 2 bytes value
            for _ in range(count):
                idx, val = struct.unpack('>Hh', data[pos:pos+entry_size])
                vector[idx] = val
                pos += entry_size
        else:  # 32-bit
            entry_size = 6  # 2 bytes index + 4 bytes float
            for _ in range(count):
                idx, val = struct.unpack('>Hf', data[pos:pos+entry_size])
                vector[idx] = val
                pos += entry_size

        return vector

    def encode_rle(self, vector: np.ndarray, threshold: float = 1e-10) -> bytes:
        """
        Encode using Run-Length + Delta encoding for clustered sparsity.
        Format: [count:2] [first_idx:2] [delta:1, value:N]*

        Best when non-zeros are clustered (saves on index storage).

        Args:
            vector: Input vector
            threshold: Values below this are considered zero

        Returns:
            Encoded bytes
        """
        # Find non-zero entries
        nonzeros = [(i, v) for i, v in enumerate(vector) if abs(v) > threshold]
        count = len(nonzeros)

        if count == 0:
            return struct.pack('>H', 0)

        if count > 65535:
            raise ValueError(f"Too many non-zeros: {count} (max 65535)")

        data = bytearray(struct.pack('>H', count))

        # First index (full 2 bytes)
        first_idx = nonzeros[0][0]
        data.extend(struct.pack('>H', first_idx))

        # Encode deltas and values
        prev_idx = first_idx
        for idx, val in nonzeros:
            delta = idx - prev_idx

            # If delta > 255, use escape code
            if delta > 254:
                data.append(255)  # Escape code
                data.extend(struct.pack('>H', delta))
            else:
                data.append(delta)

            # Encode value
            if self.value_bits == 8:
                data.extend(struct.pack('b', int(val)))
            elif self.value_bits == 16:
                data.extend(struct.pack('>h', int(val)))
            else:
                data.extend(struct.pack('>f', float(val)))

            prev_idx = idx

        return bytes(data)

    def decode_rle(self, data: bytes) -> np.ndarray:
        """Decode RLE format back to dense vector."""
        vector = np.zeros(self.dimension, dtype=np.float64)

        count = struct.unpack('>H', data[:2])[0]
        if count == 0:
            return vector

        # Read first index
        pos = 2
        current_idx = struct.unpack('>H', data[pos:pos+2])[0]
        pos += 2

        # Decode entries
        for _ in range(count):
            # Read delta
            delta = data[pos]
            pos += 1

            if delta == 255:  # Escape code for large delta
                delta = struct.unpack('>H', data[pos:pos+2])[0]
                pos += 2

            current_idx += delta

            # Read value
            if self.value_bits == 8:
                val = struct.unpack('b', data[pos:pos+1])[0]
                pos += 1
            elif self.value_bits == 16:
                val = struct.unpack('>h', data[pos:pos+2])[0]
                pos += 2
            else:
                val = struct.unpack('>f', data[pos:pos+4])[0]
                pos += 4

            vector[current_idx] = val

        return vector

    def encode_packed_bits(self, vector: np.ndarray, value_range: List[int]) -> bytes:
        """
        Encode using minimal bit packing for known value range.
        Optimal for crypto schemes with small allowed_values sets.

        Args:
            vector: Input vector (values must be in value_range or 0)
            value_range: List of allowed non-zero values (e.g., [1,2,3,4,5])

        Returns:
            Encoded bytes
        """
        # Map values to indices
        value_to_idx = {v: i+1 for i, v in enumerate(value_range)}
        value_to_idx[0] = 0  # Zero maps to 0

        # Calculate bits needed per symbol
        num_symbols = len(value_range) + 1  # +1 for zero
        bits_per_value = (num_symbols - 1).bit_length()

        # Encode non-zero positions and their values
        nonzeros = [(i, value_to_idx[int(v)]) for i, v in enumerate(vector)
                   if abs(v) > 1e-10]
        count = len(nonzeros)

        # Pack header
        data = bytearray(struct.pack('>H', count))

        # Bit-pack indices and values
        bit_buffer = 0
        bits_in_buffer = 0

        for idx, val_idx in nonzeros:
            # Pack index (11 bits for dim=2048)
            bit_buffer = (bit_buffer << 11) | idx
            bits_in_buffer += 11

            # Pack value index
            bit_buffer = (bit_buffer << bits_per_value) | val_idx
            bits_in_buffer += bits_per_value

            # Flush complete bytes
            while bits_in_buffer >= 8:
                bits_in_buffer -= 8
                data.append((bit_buffer >> bits_in_buffer) & 0xFF)

        # Flush remaining bits
        if bits_in_buffer > 0:
            data.append((bit_buffer << (8 - bits_in_buffer)) & 0xFF)

        return bytes(data)


def calculate_l2_norm(vector: np.ndarray) -> float:
    """Calculate L2 norm of a vector."""
    return sqrt(np.sum(vector ** 2))


def analyze_sparsity(vector: np.ndarray, threshold: float = 1e-10) -> dict:
    """
    Analyze vector sparsity for encoding optimization.

    Returns:
        Dictionary with sparsity metrics
    """
    nonzero_mask = np.abs(vector) > threshold
    nonzeros = vector[nonzero_mask]
    indices = np.where(nonzero_mask)[0]

    count = len(nonzeros)
    sparsity = 1.0 - (count / len(vector))

    # Calculate index clustering
    if count > 1:
        deltas = np.diff(indices)
        avg_delta = np.mean(deltas)
        max_delta = np.max(deltas)
    else:
        avg_delta = 0
        max_delta = 0

    return {
        'dimension': len(vector),
        'nonzero_count': count,
        'sparsity': sparsity,
        'l2_norm': calculate_l2_norm(vector),
        'value_range': (np.min(nonzeros), np.max(nonzeros)) if count > 0 else (0, 0),
        'unique_values': len(np.unique(nonzeros)) if count > 0 else 0,
        'avg_index_delta': avg_delta,
        'max_index_delta': max_delta,
        'clustering_ratio': avg_delta / len(vector) if count > 1 else 0
    }


def recommend_encoding(vector: np.ndarray) -> str:
    """
    Recommend best encoding method based on vector characteristics.

    Returns:
        'coo', 'rle', or 'packed'
    """
    stats = analyze_sparsity(vector)

    # If very sparse with small value range, use packed
    if stats['sparsity'] > 0.95 and stats['unique_values'] <= 16:
        return 'packed'

    # If clustered, use RLE
    if stats['clustering_ratio'] < 0.1 and stats['nonzero_count'] > 10:
        return 'rle'

    # Default to COO
    return 'coo'


def compression_ratio(original_bytes: int, compressed_bytes: int) -> float:
    """Calculate compression ratio."""
    return original_bytes / compressed_bytes if compressed_bytes > 0 else 0


if __name__ == "__main__":
    # Example usage and benchmarks
    print("Sparse Vector Encoding - Examples\n")

    # Example 1: Sparse vector with L2 norm = 45, dimension = 2048
    print("=" * 60)
    print("Example 1: L2 norm = 45, dimension = 2048, k = 50 non-zeros")
    print("=" * 60)

    np.random.seed(42)
    target_norm = 45
    k = 50  # number of non-zeros

    # Generate sparse vector
    sparse_vec = np.zeros(2048)
    indices = np.random.choice(2048, k, replace=False)
    values = np.random.randint(-5, 6, k)

    # Normalize to target L2 norm
    sparse_vec[indices] = values
    current_norm = calculate_l2_norm(sparse_vec)
    sparse_vec = sparse_vec * (target_norm / current_norm)

    actual_norm = calculate_l2_norm(sparse_vec)
    print(f"\nGenerated vector:")
    print(f"  L2 norm: {actual_norm:.2f}")
    print(f"  Non-zeros: {np.count_nonzero(sparse_vec)}")
    print(f"  Sparsity: {1 - np.count_nonzero(sparse_vec)/2048:.4f}")

    # Encode with different methods
    encoder = SparseVectorEncoder(dimension=2048, value_bits=16)

    # COO encoding
    encoded_coo = encoder.encode_coo(sparse_vec)
    decoded_coo = encoder.decode_coo(encoded_coo)

    print(f"\nCOO Encoding:")
    print(f"  Size: {len(encoded_coo)} bytes")
    print(f"  Compression: {2048*8 / len(encoded_coo):.1f}x")
    print(f"  Reconstruction error: {np.max(np.abs(sparse_vec - decoded_coo)):.6f}")

    # RLE encoding
    encoded_rle = encoder.encode_rle(sparse_vec)
    decoded_rle = encoder.decode_rle(encoded_rle)

    print(f"\nRLE Encoding:")
    print(f"  Size: {len(encoded_rle)} bytes")
    print(f"  Compression: {2048*8 / len(encoded_rle):.1f}x")
    print(f"  Reconstruction error: {np.max(np.abs(sparse_vec - decoded_rle)):.6f}")

    # Sparsity analysis
    stats = analyze_sparsity(sparse_vec)
    print(f"\nSparsity Analysis:")
    for key, value in stats.items():
        print(f"  {key}: {value}")

    print(f"\nRecommended encoding: {recommend_encoding(sparse_vec).upper()}")

    # Example 2: DNTL-style vector with small value range
    print("\n" + "=" * 60)
    print("Example 2: DNTL-style (allowed_values = [1,2,3,4,5])")
    print("=" * 60)

    allowed_values = [1, 2, 3, 4, 5]
    dntl_vec = np.zeros(256)
    k_dntl = 30
    indices_dntl = np.random.choice(256, k_dntl, replace=False)
    dntl_vec[indices_dntl] = np.random.choice(allowed_values, k_dntl)

    encoder_dntl = SparseVectorEncoder(dimension=256, value_bits=8)
    encoded_dntl = encoder_dntl.encode_coo(dntl_vec)

    print(f"\nVector stats:")
    print(f"  Dimension: 256")
    print(f"  Non-zeros: {k_dntl}")
    print(f"  L2 norm: {calculate_l2_norm(dntl_vec):.2f}")

    print(f"\nCOO Encoding (8-bit values):")
    print(f"  Size: {len(encoded_dntl)} bytes")
    print(f"  vs. dense float64: {256*8} bytes")
    print(f"  Compression: {256*8 / len(encoded_dntl):.1f}x")
