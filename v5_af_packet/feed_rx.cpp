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

#if defined(__linux__)
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  const char* iface = argc > 1 ? argv[1] : "lo";
  int port = argc > 2 ? std::atoi(argv[2]) : 15000;
  uint64_t count = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 200000;
  uint64_t gap = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 2000;
  (void)now_ns();

  int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
  if (fd < 0) { std::perror("socket(AF_PACKET) - need root / CAP_NET_RAW"); return 1; }
  int ver = TPACKET_V2;
  if (setsockopt(fd, SOL_PACKET, PACKET_VERSION, &ver, sizeof ver) < 0) { std::perror("PACKET_VERSION"); return 1; }

  tpacket_req req{};
  req.tp_block_size = 1u << 22;                       // 4 MiB blocks
  req.tp_frame_size = 2048;
  req.tp_block_nr = 8;
  req.tp_frame_nr = req.tp_block_size / req.tp_frame_size * req.tp_block_nr;
  if (setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &req, sizeof req) < 0) { std::perror("PACKET_RX_RING"); return 1; }
  size_t ring_bytes = (size_t)req.tp_block_size * req.tp_block_nr;
  auto* ring = (uint8_t*)mmap(nullptr, ring_bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
  if (ring == MAP_FAILED) { std::perror("mmap"); return 1; }

  sockaddr_ll ll{};
  ll.sll_family = AF_PACKET; ll.sll_protocol = htons(ETH_P_IP); ll.sll_ifindex = (int)if_nametoindex(iface);
  if (!ll.sll_ifindex || bind(fd, (sockaddr*)&ll, sizeof ll) < 0) { std::perror("bind"); return 1; }

  // Demo sender: ordinary UDP socket sending Msg structs to 127.0.0.1:port.
  std::thread tx([&] {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    uint64_t next = now_ns();
    for (uint64_t i = 0; i < count; ++i) {
      while (now_ns() < next) cpu_relax();
      next += gap;
      Msg m{now_ns(), i, 0, 0, i};
      sendto(s, &m, sizeof m, 0, (sockaddr*)&a, sizeof a);
    }
    close(s);
  });

  std::vector<uint32_t> lat;
  lat.reserve(count);
  uint64_t expect = 0, dup_or_gap = 0;
  size_t frame = 0;
  uint64_t idle_since = now_ns();
  while (expect < count && now_ns() - idle_since < 5'000'000'000ull) {     // give up after 5 s of silence
    auto* hdr = (tpacket2_hdr*)(ring + frame * req.tp_frame_size);
    if (!(__atomic_load_n(&hdr->tp_status, __ATOMIC_ACQUIRE) & TP_STATUS_USER)) { cpu_relax(); continue; }
    idle_since = now_ns();
    uint64_t t_rx = now_ns();
    auto* sll = (sockaddr_ll*)((uint8_t*)hdr + TPACKET_ALIGN(sizeof(tpacket2_hdr)));
    const uint8_t* pkt = (const uint8_t*)hdr + hdr->tp_mac;   // Ethernet header start
    const uint8_t* ip = pkt + ETH_HLEN;
    size_t ihl = (ip[0] & 0x0F) * 4;
    if (sll->sll_pkttype != PACKET_OUTGOING && ip[9] == IPPROTO_UDP && hdr->tp_snaplen >= ETH_HLEN + ihl + 8 + sizeof(Msg)) {
      const uint8_t* udp = ip + ihl;
      if (ntohs(*(const uint16_t*)(udp + 2)) == port) {
        Msg m;
        std::memcpy(&m, udp + 8, sizeof m);
        if (m.seq == expect) { lat.push_back(t_rx > m.t_ns ? (uint32_t)(t_rx - m.t_ns) : 0); ++expect; }
        else ++dup_or_gap;                                    // loopback duplicate, or a real loss
      }
    }
    __atomic_store_n(&hdr->tp_status, TP_STATUS_KERNEL, __ATOMIC_RELEASE);  // hand the frame back
    frame = frame + 1 == req.tp_frame_nr ? 0 : frame + 1;
  }
  tx.join();
  if (lat.empty()) { std::puts("no packets received"); return 1; }
  std::sort(lat.begin(), lat.end());
  auto at = [&](double f) { return lat[std::min(lat.size() - 1, (size_t)(f * lat.size()))]; };
  std::printf("received %zu/%llu  skipped(dup/gap)=%llu\n", lat.size(), (unsigned long long)count, (unsigned long long)dup_or_gap);
  std::printf("send->userspace  p50=%uns p99=%uns p99.9=%uns max=%uns\n", at(.5), at(.99), at(.999), lat.back());
}
#else
int main() { std::puts("v5 is Linux only (AF_PACKET / PACKET_MMAP)."); }
#endif
