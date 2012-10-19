#!/usr/bin/env python

import sys
import random

server = []
fin = open("server")
for line in fin:
    server.append(line.strip())
fin.close()

num_cores = int(sys.argv[1])
num_workloads = int(sys.argv[2])
workloads = []

while num_workloads > 0:
    avail_cores = num_cores
    avail_bench = server[:]
    workload = []

    while avail_cores > 0:
        chosen = random.choice(avail_bench)
        workload.append(chosen)
        avail_bench.remove(chosen)
        avail_cores = avail_cores - 1

    string = "-".join(sorted(workload))
    if string not in workloads:
        workloads.append(string)
        num_workloads = num_workloads - 1

for workload in workloads:
    print workload
