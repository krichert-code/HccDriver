#include <linux/filter.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define NETLINK_HCC 31

// struct nimsghdr {
//_u32 nimsg len; /* Length of message including header */
//_u16 nimsg_type; /* Message content */
//_u16 nimsg_flags; /* Additional flags */
//_u32 nimsg_seq; /* Sequence number */
//_u32 nimsg_pid; /* Sending process port ID */
// PAYLOAD
// };
struct sock_filter bpf_code[] = {
    { 0x30, 0, 0, 0x00000010},
    {0x15, 0, 1, 0x00000041},
    {0x6, 0, 0, 0xffffffff},
    { 0x6, 0, 0, 0x00000000},
};

struct sock_fprog bpf_prog = {
    sizeof(bpf_code)/sizeof(struct sock_filter),
    bpf_code,
};

int main() {

    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    struct msghdr msg;
    int sock = socket (AF_NETLINK, SOCK_RAW, NETLINK_HCC);

    if (sock < 0) {
        perror("socket");
        return 1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &bpf_prog, sizeof(bpf_prog)) < 0) {
        perror("setsockopt");
        return 1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; // kernel
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; // kernel
    dest_addr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(8));
    memset(nlh, 0, NLMSG_SPACE(8));
    nlh->nlmsg_len = NLMSG_LENGTH(8);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    strcpy(NLMSG_DATA(nlh), "Test");
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // // wysyłanie do kernela
    sendmsg(sock, &msg, 0);
    
    // // odbiór odpowiedzi
    while(1) {
        recvmsg(sock, &msg, 0);
        printf("Received from kernel: %s\n", (char *)NLMSG_DATA(nlh));
    }

    free(nlh);
    close(sock);
    return 0;
}