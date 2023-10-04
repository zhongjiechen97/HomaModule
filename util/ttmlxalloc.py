#!/usr/bin/python3

# Copyright (c) 2022 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""
Scans a time trace file for entries generated by the Mellanox driver about
packet allocations and deallocations (because the per-channel cache
overflowed or underflowed). If the file argument is present, it specifies
the name of the time trace file; otherwise time traces read from standard
input. If --verbose is specified, then the program outputs a new time trace
where adjacent allocate/free entries have been collapsed into a single entry
for ease of reading.  Otherwise it just prints statistics about the cost
of allocation and deallocation
Usage: ttmlxalloc.py [--verbose] [file]
"""

from __future__ import division, print_function
from glob import glob
from optparse import OptionParser
import math
import os
import re
import string
import sys

verbose = False
f = sys.stdin

# A dictionary where keys are core ids, and each value is the number
# of consecutive time trace entries for that core that are for page
# allocations/frees.
num_allocs = {}
num_frees = {}

# A dictionary where keys are core ids, and each value is the time of
# the first page allocation/free entry in the current batch for that core.
first_alloc_time = {}
first_free_time = {}

# A dictionary where keys are core ids, and each value is the time of
# the most recent page allocation/free for that core.
last_alloc_time = {}
last_free_time = {}

# Time of previous time trace record that was printed.
prev_time = 0

# Each entry in this list is a count of the number of pages allocated/freed
# in one batch.
alloc_counts = []
free_counts = []

# Each entry in this list is the time consumed by a single batch of page
# allocations/frees.
alloc_times = []
free_times = []

# Dictionary whose keys are the ids of all the distinct RPCs seen in
# the trace.
ids = {}

if (len(sys.argv) == 2) and (sys.argv[1] == "--help"):
    print("Usage: %s [--stats] [file]" % (sys.argv[0]))
    sys.exit(0)
if (len(sys.argv) >= 2) and (sys.argv[1] == "--verbose"):
    verbose = True
    sys.argv.pop(1)
if len(sys.argv) >= 2:
    f = open(sys.argv[1])

for line in f:
    match = re.match(' *([0-9.]+) us .* \[(C[0-9]+)\] (.*)', line)
    if not match:
        if verbose:
            print(line)
        continue
    time = float(match.group(1))
    core = match.group(2)
    msg = match.group(3)
    if not core in num_allocs:
        num_allocs[core] = 0
        num_frees[core] = 0
    match = re.match('.*id ([0-9.]+)', msg)
    if match:
        ids[match.group(1)] = 1

    if 'mlx starting page alloc' in msg:
        if num_allocs[core] == 0:
            first_alloc_time[core] = time
        last_alloc_time[core] = time
        num_allocs[core] += 1
        continue

    if 'mlx starting page release' in msg:
        if num_frees[core] == 0:
            first_free_time[core] = time
        last_free_time[core] = time
        num_frees[core] += 1
        continue

    if num_allocs[core] != 0:
        if verbose:
            print("%9.3f us (+%8.3f us) [%s] mlx allocated %d pages (%.1f us)" % (
                    time, time - prev_time, core, num_allocs[core],
                    last_alloc_time[core] - first_alloc_time[core]))
        alloc_counts.append(num_allocs[core])
        alloc_times.append(last_alloc_time[core] - first_alloc_time[core])
        num_allocs[core] = 0
        prev_time = time

    if num_frees[core] != 0:
        if verbose:
            print("%9.3f us (+%8.3f us) [%s] mlx freed %d pages (%.1f us)" % (
                    time, time - prev_time, core, num_frees[core],
                    last_free_time[core] - first_free_time[core]))
        free_counts.append(num_frees[core])
        free_times.append(last_free_time[core] - first_free_time[core])
        num_frees[core] = 0
        prev_time = time

    if verbose:
        print("%9.3f us (+%8.3f us) [%s] %s" % (time, time - prev_time, core, msg))

if verbose:
    sys.exit(0)

print("Total number of RPCs: %6d" % (len(ids)))
print("Total elapsed time:   %8.1f us" % (prev_time))
print("")
if len(alloc_counts) == 0:
    print("No page allocations")
else:
    print("Page allocations:")
    print("  Total pages:        %6d" % (sum(alloc_counts)))
    print("  Batches:             %5d" % (len(alloc_counts)))
    print("  Average batch size:  %7.1f" % (sum(alloc_counts)/len(alloc_counts)))
    print("  Average batch time:  %7.1f us" % (sum(alloc_times)/len(alloc_counts)))
    print("  Alloc time per RPC:  %7.1f us" % (sum(alloc_times)/len(ids)))
    print("  Total time:          %7.1f us (%.3f core)" % (sum(alloc_times),
            sum(alloc_times)/prev_time))
if len(free_counts) == 0:
    print("No page frees")
else:
    print("Page frees:")
    print("  Total pages:        %6d" % (sum(free_counts)))
    print("  Batches:             %5d" % (len(free_counts)))
    print("  Average batch size:  %7.1f" % (sum(free_counts)/len(free_counts)))
    print("  Average batch time:  %7.1f us" % (sum(free_times)/len(free_counts)))
    print("  Free time per RPC:   %7.1f us" % (sum(free_times)/len(ids)))
    print("  Total time:          %7.1f us (%.3f core)" % (sum(free_times),
            sum(free_times)/prev_time))