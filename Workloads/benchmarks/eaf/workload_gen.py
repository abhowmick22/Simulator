#!/usr/bin/env python

import sys
import random

# ----------------------------------------------------------------------
# Workloads
# ----------------------------------------------------------------------

benchmarks = []
intensity = {}
utility = {}

server = []

fin = open("all")
for line in fin:
    (bench, intense, util) = line.split()
    benchmarks.append(bench)
    intensity[bench] = int(intense)
    utility[bench] = int(util)
fin.close()


num_cores = int(sys.argv[1])
total_intensity = int(sys.argv[2])
total_utility = int(sys.argv[3])
num_workloads = int(sys.argv[4])
workloads = []


# emit as many workloads as needed

while num_workloads > 0:

    avail_benchmarks = benchmarks[:]
    avail_intensity = total_intensity
    avail_utility = total_utility
    avail_cores = num_cores
    workload = []
    discarded = False

    while avail_cores > 0:

        # filter benchmarks that are above required intensity and utility
        avail_benchmarks = [bench for bench in avail_benchmarks \
                                if intensity[bench] <= avail_intensity and \
                                utility[bench] <= avail_utility]

        # if only one core is available, then remove benchmarks that
        # cannot achieve avail intensity and utility
        if avail_cores == 1:
            avail_benchmarks = [bench for bench in avail_benchmarks \
                                    if intensity[bench] >= avail_intensity and \
                                    utility[bench] >= avail_utility]

        if len(avail_benchmarks) == 0:
            discarded = True
            break
            
        # pick a random benchmark from the available list and add it
        # to the workload
        chosen = random.choice(avail_benchmarks)
        workload.append(chosen)
        avail_benchmarks.remove(chosen)
        avail_cores = avail_cores - 1
        avail_utility = avail_utility - utility[chosen]
        avail_intensity = avail_intensity - intensity[chosen]

    if not discarded:
        string = "-".join(sorted(workload))
        if string not in workloads:
            workloads.append(string)
            print string
            num_workloads = num_workloads - 1

#for workload in workloads:
#    print workload
    
