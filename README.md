# dntl

python3 dntl-dsa-nat.py -c 1 # Level 1, 80B Public Keys and Signatures\
python3 dntl-dsa-nat.py -c 3 # Level 3, 152B Public Keys and Signatures\
python3 dntl-dsa-nat.py -c 5 # Level 5, 288B Public Keys and Signatures

# Overview

At a high level, this is k-DSP. It's not a Euclidean lattice based scheme, it's actually based on a natural lattice. It's closed, periodic, linear and fits standard lattice theory. An average case natural ISIS to worst case SVP reduction will be forthcoming. For now, there are k chained independent lattices, with the core instance being a natural ISIS problem. The core instance is full rank with a secret that maps to short Euclidean secret, so there's not much to say about that. The chaining function is based on a non-inverting NTT, so the output from each instance is transformed in an alternate lattice domain, and then returned to the input domain before becoming the hidden vector in the next instance. 

You may notice the non-core instances are slightly less than full rank. This is to create an instance where:\
	t=Ax mod q;\
has more than one valid x solution. The goal is to find the right solution for the outer instances via some inversion technique and get to the core ISIS instance. From there, extract the short secret!

# Warning
Don't use this for anything serious, obviously, it's Python. But I am curious to see where it breaks. Good luck!

As of now, nobody has pointed out an issue with the core design. I'll remove this note if somebody does. 

Send feedback to info@quantumshield.us
