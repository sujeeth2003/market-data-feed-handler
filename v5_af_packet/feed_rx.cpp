// v5: kernel-bypass-style receive with AF_PACKET + PACKET_MMAP (TPACKET_V2).
//
// The kernel writes received frames straight into a ring that is memory-mapped
// into this process. The receiver just polls frame headers: no recvfrom()
// syscall per packet, no copy out of the socket buffer, and no special NIC or
// driver (unlike DPDK / AF_XDP / Onload). It still goes through the kernel's
// driver and interrupt path, so it is a step toward kernel bypass, not the end.
//
// Linux only, needs CAP_NET_RAW (run as root). Demo mode sends UDP on the
// loopback interface from a second thread and measures send -> user-space time.
//
//   sudo ./feed_rx [iface=lo] [port=15000] [count=200000] [gap_ns=2000]
//
// NOTE: compiled and cross-checked for Linux but not executed on this
// development machine (Windows). Treat runtime numbers as yet to be measured.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../common/clock.hpp"
#include "../common/msg.hpp"
#include "../common/platform.hpp"

