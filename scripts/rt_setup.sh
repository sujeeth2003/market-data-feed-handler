#!/bin/sh
# Linux host preparation for v6. Read before running: several of these need root
# and a reboot, and isolcpus/nohz_full take those cores away from the scheduler.
#
# 1. Boot parameters (edit GRUB_CMDLINE_LINUX in /etc/default/grub, run update-grub, reboot).
#    Isolate cores 1-3 from the scheduler, stop timer ticks on them, keep IRQs off them:
#      isolcpus=1-3 nohz_full=1-3 rcu_nocbs=1-3 irqaffinity=0
#
# 2. Reserve 2 MiB huge pages (each v6 ring needs one 2 MiB page; reserve a few more):
#      echo 64 | sudo tee /proc/sys/vm/nr_hugepages
#
# 3. Allow mlock and real-time priority for your user (/etc/security/limits.conf):
#      youruser  -  memlock  unlimited
#      youruser  -  rtprio   99
#
# 4. Pin the CPU frequency so a governor change does not add jitter:
#      sudo cpupower frequency-set -g performance
#
# 5. Run with the benchmark thread pinning flag (producer p -> core p+1, consumer -> core 0):
#      ./build/bench_rt 2 500000 1000 1
echo "This file is documentation; nothing was changed."
grep -E 'HugePages_(Total|Free)|Hugepagesize' /proc/meminfo 2>/dev/null || true
cat /sys/devices/system/cpu/isolated 2>/dev/null | sed 's/^/isolated cpus: /' || true
