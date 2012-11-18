#!/usr/bin/python

import random
import sys

fname = sys.argv[1]
count = int(sys.argv[2])

f = open(fname)
l = f.read().split()
s = random.sample(l, count)
print '\n'.join(s)
