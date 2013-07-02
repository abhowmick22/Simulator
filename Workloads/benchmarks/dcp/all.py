#!/usr/bin/env python

import sys
import random

f = open(sys.argv[1])
l = f.read().split()

for i in range(len(l)):
    for j in range(i, len(l)):
        print '%s-%s' % (l[i], l[j])
