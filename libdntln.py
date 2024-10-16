import random
import numpy as np
from math import log2
from collections import Counter
import os


def generate_random_bytes(x):
    return os.urandom(x)


def qary(v, q):
    # Ensure v is a numpy array
    v = np.array(v)

    # Transform each element to ensure it is within the range [-q/2, q/2]
    transformed_v = ((v + q // 2) % q) - q // 2

    return transformed_v


# Function to calculate Shannon entropy
def calculate_entropy(arr):
    bitstring = "".join(format(x, "08b") for x in arr)
    # Count the frequency of each bit (or symbol)
    counts = Counter(bitstring)
    total = len(bitstring)

    # Calculate Shannon entropy
    entropy = -sum((count / total) * log2(count / total) for count in counts.values())
    return entropy


def calculate_norm(vector):
    return np.linalg.norm(vector)


# NOTE: Constant time lookup table replaces this in real implementations
################################################################
def pointwise_multiplication2(vec1, vec2, modulus):
    return [(x * y) % modulus for x, y in zip(vec1, vec2)]

def pointwise_multiplication(vec1, vec2, modulus=257):
    result = [(x * y) % modulus for x, y in zip(vec1, vec2)]

    # Adjust for additive identity: Map any result of 0 to modulus (257)
    result = [modulus if r == 0 else r for r in result]

    return result

def pointwise_addition2(vec1, vec2, modulus):
    return [(x + y) % modulus for x, y in zip(vec1, vec2)]


def pointwise_addition(vec1, vec2, modulus=257):
    result = [(x + y) % modulus for x, y in zip(vec1, vec2)]

    # Adjust for additive identity: Map any result of 0 to modulus (257)
    result = [modulus if r == 0 else r for r in result]

    return result

def gaussian_select_from_set(
    mu=3, sigma=3, s=1, allowed_values=[-9, -8, -7, -6, -5, -4, -3, 3, 4, 5, 6, 7, 8, 9]
):
    # Generate a random number from a Gaussian distribution (mean=0, std=sigma)
    # random.seed(s)
    gaussian_value = random.gauss(mu, sigma)

    # Find the closest value from the allowed set
    closest_value = min(allowed_values, key=lambda x: abs(x - gaussian_value))

    return closest_value
