# dntl

dntl-dsa-gnat.py -c 1 # Level 1, 80B Public Keys and Signatures\
dntl-dsa-gnat.py -c 3 # Level 3, 88B Public Keys and Signatures\
dntl-dsa-gnat.py -c 5 # Level 5, 160B Public Keys and Signatures

# Overview

At a high level, this is k-DSP. It's not exactly a lattice based scheme, but it started off as one. There are k chained independent 'lattices', with the core instance being a normal ISIS problem. The core instance is full rank with a properly short secret so there's not much to say about that. The chaining function is based on a non-inverting NTT, so the output from instance is transformed in an alternate lattice domain, and then returned to the input domain before becoming the hidden vector in the next instance. 

You may notice the non-core instances are slightly less than full rank. This is to create an instance where:\
	t=Ax mod q;\
has more than one valid x solution. The goal is to find the right solution for the outer instances via some inversion technique and get to the core ISIS instance. From there, extract the short secret!



Send feedback to hel0hel0@hotmail.com