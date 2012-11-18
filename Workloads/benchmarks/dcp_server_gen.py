#!/usr/bin/env python

import sys
import random

# ----------------------------------------------------------------------
# Workloads
# ----------------------------------------------------------------------

benchmarks = []
cb = {} # cache benefit
pb = {} # pref benefit


fin = open("server")
for line in fin:
    (bench, bcb, bpb) = line.strip().split()
    benchmarks.append(bench)
    cb[bench] = int(bcb)
    pb[bench] = int(bpb)
fin.close()


num_cores = int(sys.argv[1])
num_workloads = int(sys.argv[2])
workloads = []


# emit as many workloads as needed

while num_workloads > 0:

    avail_benchmarks = benchmarks[:]
    avail_cores = num_cores
    workload = []
    discarded = False

    while avail_cores > 0:

        # pick a random benchmark from the available list and add it
        # to the workload
        chosen = random.choice(avail_benchmarks)
        workload.append(chosen)
        avail_cores = avail_cores - 1

    if not discarded:
        string = "-".join(sorted(workload))
        if string not in workloads:
            workloads.append(string)
            print string
            num_workloads = num_workloads - 1

#for workload in workloads:
#    print workload
    
