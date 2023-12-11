# adaptivemm
## A userspace daemon for proactive memory management

## Overview
adaptivemm offers montioring and tuning of various aspects of system
memory. adaptivemm consists of modules that each offer a different
set of functionality.

Free memory management module in adaptivemm monitors current state
of free pages overall and free pages of each order. Based upon
current rate of free pages consumption and memory fragmentation, it
predicts if system is likely to run out of memory or if memory will
become severely fragmented in near future. If so, it adjusts
watermarks to force memory reclamation if system is about to run out
of memory. If memory is predicted to become severely fragmented, it
triggers compaction in the kernel. The goal is to avert memory
shortage and/or fragmentation by taking proactive measures. To
arrive at this prediction, adaptivemm samples free pages of each
order on each node periodically and fits a straight line using
method of least squares to these sample points. It also computes
current reclamation rate by monitoring `/proc/vmstat`. The equation
derived for best fit line is used to compute when free memory
exhaustion will occur taking into account current reclamation rate.
If this exhaustion is imminent in near future, watermarks are
adjusted to initiate reclamation.

Negative dentry management module monitors and adjusts the the
negative dentry limit on the system. Negative dentry limit is
interpreted by kernel as a fraction of total system memory. When a
large number of hugepages are allocated on the system, a cap as a
fraction of total system memory can be too high and can result in
out-of-memory condition since hugepages are not swappable.
adaptivemm can make sure that negative dentry cap is always set to
accomplish what end user intends the cap to be as a fraction
irrespective of number of hugepages allocated on the system.

Memory leak detection module looks for abnormal changes to free
memory. It uses data reported in /proc/meminfo to determine how much
memory is in use and based upon that number, does the amount of free
memory look reasonable.

## How it works

adaptivemm free memory management module samples `/proc/buddyinfo`
to monitor the total available free memory and the fraction thereof
that, whilst notionally free, requires compaction before it can be
used.  At any given moment, the difference between these two
quantities is the amount of memory that is free for immediate use:
once this is exhausted, subsequent allocations will stall whilst a
portion of the fragmented memory is compacted.  The program
calculates trends for both the total and fragmented free memory and,
by extrapolation, determines whether exhaustion is imminent.  At the
last possible moment, i.e. only once it is absolutely necessary, the
program initiates compaction with a view to recovering the
fragmented memory before it is required by subsequent allocations.
It initiates compaction by writing 1 to
`/sys/devices/system/node/node%d/compact`. If number of free pages
is expected to be exhausted, it looks at the number of inactive
pages in cache buffer to determine if changing watermarks can result
in meaningful number of pages reclaimed. It adjusts watermark by
changing watermark scale factor in
`/proc/sys/vm/watermark_scale_factor`.

## Prerequisites

adaptivemm must be run as root and must have access to following files:

`/proc/vmstat`
`/proc/buddyinfo`
`/proc/zoneinfo`
'/proc/kpagecount'
'/proc/kpageflags'
`/proc/sys/vm/watermark_scale_factor`
`/sys/devices/system/node/node%d/compact`
'/proc/sys/fs/negative-dentry-limit'

adaptivemm daemon can be run standalone or can be started
automatically by systemd/init.

## Usage

adaptivemm supports three levels of aggressiveness - 1 = Low, 2 =
Normal (default), 3 = High. Aggressiveness level dictates how often
adaptivemm will sample system state and how aggressively it will
tune watermark scale factor.

In simplest form, adaptivemmd can be started with:

	$ adaptivemmd

Aggressiveness level can be changed with:

	$ adaptivemmd -a 3

If a maximum gap allowed between low and high watermarks as
adaptivemm tunes watermarks, is desired, it can be specified with
a -m flag where the argument is number of GB:

	$ adaptivemmd -m 10

## Developer Resources

To develop for adaptivemm, obtain the source code from git repository at https://github.com/oracle/adaptivemm or from the source package. Source code is broken down into two primary files - adaptivemmd.c contains the main driving code for the daemon which does all the initialization and takes action in response to the results from prediction algorithm, predict.c contains the core of prediction algorithm which uses least square fit method to calculate a trend line for memory consumption.

## Prerequisite to building

adaptivemm requires only the basic build tools - gcc compiler and linker, and make.

## Building

Run `make all` to build the adaptivemmd daemon.

Run `make clean` to remove binaries and intermediate files generated by build process.

## Installation

Copy adaptivemmd binary to a directory appropriate for your system, typically `/usr/sbin`. adaptivemm can use a configuration file as well which is `/etc/sysconfig/adaptivemmd` on rpm based system and `/etc/default/adaptivemmd` on deb based systems. Here is a sample configuration file:


	# Configuration file for adaptivemmd
	#
	# Options in this file can be overridden with command line options
	
	# verbosity level (1-5)
	VERBOSE=0
	
	# Aggressiveness level for adaptivemmd (1-3)
	AGGRESSIVENESS=2
	
	# ============================
	# Free page management section
	# ============================
	# Enable management of free pages through watermarks tuning
	ENABLE_FREE_PAGE_MGMT=1
	
	# Maximum gap between low and high watermarks (in GB)
	# MAXGAP=5
	
	# ==================================
	# Negative dentry management section
	# ==================================
	# Enable management of negative dentry cap
	ENABLE_NEG_DENTRY_MGMT=1
	
	# Cap for memory consumed by negative dentries as a fraction of 1000.
	# Range of values supported by kernel is 1-100.
	# NOTE: for kernels with support for this functionality
	#	(Hint: look for /proc/sys/fs/negative-dentry-limit)
	NEG_DENTRY_CAP=15

	# ==============================
	# Memory leack detection section
	# ==============================
	# Enable checks for possible memory leaks
	ENABLE_MEMLEAK_CHECK=1


## Documentation

Source code includes documentation in form of a man page. This man page is contained in file `adaptivemmd.8`. If adaptivemm was installed as a package on your system, man page should be available as standard man page with `man adaptivemmd` command. If adaptivemm was not installed as a package, `adaptivemmd.8` file can be displayed as man page with `nroff -man adaptivemmd.8`.

## Contributing

This project welcomes contributions from the community. Before submitting a pull request, please [review our contribution guide](./CONTRIBUTING.md)

## Security

Please consult the [security guide](./SECURITY.md) for our responsible security vulnerability disclosure process

## License

Copyright (c) 2019, 2023 Oracle and/or its affiliates.

Released under the GNU Public License version 2
