#!/usr/bin/env python

import sys
import random

cores = int(sys.argv[1])
ccount = int(sys.argv[2])
pcount = cores - ccount
count = int(sys.argv[3])

with open('c') as f:
    clist = f.read().split()

with open('nc-p') as f:
    plist = f.read().split()


workloads = []
n = 0

while n < count:
    c = 0
    p = 0
    pbench = []
    cbench = []
    while c < ccount:
        cbench.append(random.choice(clist))
        c += 1
    while p < pcount:
        pbench.append(random.choice(plist))
        p += 1
        
    workload = '-'.join(sorted(cbench) + sorted(pbench))
    if workload not in workloads:
        print workload
        n += 1
        workloads.append(workload)
