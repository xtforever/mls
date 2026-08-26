#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define MAX_FLOWS 100000

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;

    uint64_t packets;
    uint64_t bytes;

    time_t first_seen;
    time_t last_seen;
} flow_t;

static flow_t flows[MAX_FLOWS];
static size_t flow_count = 0;

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

/* ---------- endian helpers ---------- */

static uint16_t read_u16(const unsigned char *p, int swap)
{
    uint16_t v;
    memcpy(&v, p, sizeof(v));

    if (swap)
        v = (uint16_t)((v >> 8) | (v << 8));

    return v;
}

static uint32_t read_u32(const unsigned char *p, int swap)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));

    if (swap) {
        v = ((v & 0x000000ffU) << 24) |
            ((v & 0x0000ff00U) << 8)  |
            ((v & 0x00ff0000U) >> 8)  |
            ((v & 0xff000000U) >> 24);
    }

    return v;
}

static uint32_t ntoh32(uint32_t v)
{
    return ntohl(v);
}

/* ---------- flow handling ---------- */

static flow_t *find_flow(
    uint32_t src_ip,
    uint32_t dst_ip,
    uint8_t protocol,
    uint16_t src_port,
    uint16_t dst_port)
{
    for (size_t i = 0; i < flow_count; i++) {
        flow_t *f = &flows[i];

        if (f->src_ip == src_ip &&
            f->dst_ip == dst_ip &&
            f->protocol == protocol &&
            f->src_port == src_port &&
            f->dst_port == dst_port) {
            return f;
        }
    }

    if (flow_count >= MAX_FLOWS)
        return NULL;

    flow_t *f = &flows[flow_count++];

    memset(f, 0, sizeof(*f));

    f->src_ip = src_ip;
    f->dst_ip = dst_ip;
    f->protocol = protocol;
    f->src_port = src_port;
    f->dst_port = dst_port;

    return f;
}

static void process_packet(
    const unsigned char *packet,
    uint32_t captured_len,
    uint32_t original_len,
    time_t timestamp)
{
    /*
     * Ethernet header:
     *
     * dst MAC   6
     * src MAC   6
     * EtherType 2
     */
    if (captured_len < 14)
        return;

    uint16_t ethertype =
        ((uint16_t)packet[12] << 8) |
        packet[13];

    /*
     * IPv4 only.
     */
    if (ethertype != 0x0800)
        return;

    const unsigned char *ip = packet + 14;

    if (captured_len < 14 + 20)
        return;

    uint8_t version = ip[0] >> 4;
    uint8_t ihl = (ip[0] & 0x0f) * 4;

    if (version != 4 || ihl < 20)
        return;

    if (captured_len < 14 + ihl)
        return;

    uint8_t protocol = ip[9];

    /*
     * TCP = 6
     * UDP = 17
     */
    if (protocol != 6 && protocol != 17)
        return;

    uint32_t src_ip;
    uint32_t dst_ip;

    memcpy(&src_ip, ip + 12, sizeof(src_ip));
    memcpy(&dst_ip, ip + 16, sizeof(dst_ip));

    const unsigned char *transport = ip + ihl;

    if (captured_len < 14 + ihl + 4)
        return;

    uint16_t src_port =
        ((uint16_t)transport[0] << 8) |
        transport[1];

    uint16_t dst_port =
        ((uint16_t)transport[2] << 8) |
        transport[3];

    flow_t *f = find_flow(
        src_ip,
        dst_ip,
        protocol,
        src_port,
        dst_port
    );

    if (!f)
        return;

    f->packets++;
    f->bytes += original_len;

    if (f->first_seen == 0)
        f->first_seen = timestamp;

    f->last_seen = timestamp;
}

/* ---------- output ---------- */

static const char *protocol_name(uint8_t protocol)
{
    switch (protocol) {
        case 6:
            return "TCP";
        case 17:
            return "UDP";
        default:
            return "?";
    }
}

static void print_flows(void)
{
    printf(
        "%-15s %-15s %-5s %6s %6s %12s %14s\n",
        "SRC",
        "DST",
        "PROTO",
        "SPORT",
        "DPORT",
        "PACKETS",
        "BYTES"
    );

    printf(
        "--------------------------------------------------------------------------\n"
    );

    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];

    for (size_t i = 0; i < flow_count; i++) {
        flow_t *f = &flows[i];

        inet_ntop(
            AF_INET,
            &f->src_ip,
            src,
            sizeof(src)
        );

        inet_ntop(
            AF_INET,
            &f->dst_ip,
            dst,
            sizeof(dst)
        );

        printf(
            "%-15s %-15s %-5s %6u %6u %12llu %14llu\n",
            src,
            dst,
            protocol_name(f->protocol),
            f->src_port,
            f->dst_port,
            (unsigned long long)f->packets,
            (unsigned long long)f->bytes
        );
    }
}

int main(void)
{
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /*
     * PCAP global header
     */
    unsigned char gh[24];

    if (fread(gh, 1, sizeof(gh), stdin) != sizeof(gh)) {
        fprintf(stderr, "failed to read PCAP header\n");
        return EXIT_FAILURE;
    }

    uint32_t magic =
        ((uint32_t)gh[0]) |
        ((uint32_t)gh[1] << 8) |
        ((uint32_t)gh[2] << 16) |
        ((uint32_t)gh[3] << 24);

    int swap;

    /*
     * PCAP magic numbers:
     *
     * 0xa1b2c3d4 = normal
     * 0xd4c3b2a1 = byte swapped
     */
    if (magic == 0xa1b2c3d4) {
        swap = 0;
    } else if (magic == 0xd4c3b2a1) {
        swap = 1;
    } else {
        fprintf(stderr, "unsupported PCAP format: 0x%08x\n", magic);
        return EXIT_FAILURE;
    }

    uint32_t network =
        read_u32(gh + 20, swap);

    /*
     * Ethernet = DLT 1
     */
    if (network != 1) {
        fprintf(
            stderr,
            "unsupported link type: %u (only Ethernet supported)\n",
            network
        );
        return EXIT_FAILURE;
    }

    while (running) {
        unsigned char ph[16];

        size_t n = fread(ph, 1, sizeof(ph), stdin);

        if (n == 0)
            break;

        if (n != sizeof(ph)) {
            fprintf(stderr, "truncated packet header\n");
            break;
        }

        uint32_t ts_sec =
            read_u32(ph + 0, swap);

        uint32_t incl_len =
            read_u32(ph + 8, swap);

        uint32_t orig_len =
            read_u32(ph + 12, swap);

        /*
         * Safety limit.
         */
        if (incl_len > 16 * 1024 * 1024) {
            fprintf(stderr, "invalid packet size: %u\n", incl_len);
            break;
        }

        unsigned char *packet = malloc(incl_len);

        if (!packet) {
            fprintf(stderr, "out of memory\n");
            break;
        }

        if (fread(packet, 1, incl_len, stdin) != incl_len) {
            free(packet);
            break;
        }

        process_packet(
            packet,
            incl_len,
            orig_len,
            (time_t)ts_sec
        );

        free(packet);
    }

    fprintf(
        stderr,
        "captured %zu unique flows\n",
        flow_count
    );

    print_flows();

    return EXIT_SUCCESS;
}
