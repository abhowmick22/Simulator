#!/usr/bin/env python

import sys
import random

# ----------------------------------------------------------------------
# Workloads
# ----------------------------------------------------------------------

benchmarks = []
cb = {} # cache benefit
pb = {} # pref benefit


fin = open("all")
for line in fin:
    (bench, bcb, bpb) = line.strip().split()
    benchmarks.append(bench)
    cb[bench] = int(bcb)
    pb[bench] = int(bpb)
fin.close()


num_cores = int(sys.argv[1])
total_cb = int(sys.argv[2])
total_pb = int(sys.argv[3])
num_workloads = int(sys.argv[4])
workloads = []


# emit as many workloads as needed

while num_workloads > 0:

    avail_benchmarks = benchmarks[:]
    avail_cb = total_cb
    avail_pb = total_pb
    avail_cores = num_cores
    workload = []
    discarded = False

    while avail_cores > 0:

        # filter benchmarks that are above required cb and pb
        avail_benchmarks = [bench for bench in avail_benchmarks \
                                if cb[bench] <= avail_cb and \
                                pb[bench] <= avail_pb]

        # if only one core is available, then remove benchmarks that
        # cannot achieve avail cb and pb
        if avail_cores == 1:
            avail_benchmarks = [bench for bench in avail_benchmarks \
                                    if cb[bench] >= avail_cb and \
                                    pb[bench] >= avail_pb]

        if len(avail_benchmarks) == 0:
            discarded = True
            break
            
        # pick a random benchmark from the available list and add it
        # to the workload
        chosen = random.choice(avail_benchmarks)
        workload.append(chosen)
        avail_cores = avail_cores - 1
        avail_cb -= cb[chosen]
        avail_pb -= pb[chosen]

    if not discarded:
        string = "-".join(sorted(workload))
        if string not in workloads:
            workloads.append(string)
            print string
            num_workloads = num_workloads - 1

#for workload in workloads:
#    print workload
    
