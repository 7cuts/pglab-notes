#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/in.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <linux/tcp.h>

typedef struct {
		struct nlmsghdr nlh;
		struct inet_diag_req_v2 idr;
} Request;

static int
send_query(int fd)
{
	struct sockaddr_nl nladdr = {
		.nl_family = AF_NETLINK
	};
	
	
	Request req = {
		.nlh = {
			.nlmsg_len = sizeof(req),
			.nlmsg_type = SOCK_DIAG_BY_FAMILY,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
		},
		.idr = {
			.sdiag_family = AF_INET,
			.sdiag_protocol = IPPROTO_TCP, 
			.idiag_ext = (1 << (INET_DIAG_SKMEMINFO - 1)) 
				| (1 << (INET_DIAG_INFO - 1)),
			.pad = 0,
			.idiag_states = 0xFFFFFFFF, // all states
		}
	};
	struct iovec iov = {
		.iov_base = &req,
		.iov_len = sizeof(req)
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	for (;;) {
		if (sendmsg(fd, &msg, 0) < 0) {
			if (errno == EINTR)
				continue;
			perror("sendmsg");
			return -1;
		}
		return 0;
	}
}

static const char *state_str(__u8 s) {
    switch (s) {
        case 1:  return "ESTABLISHED";
        case 2:  return "SYN_SENT";
        case 3:  return "SYN_RECV";
        case 4:  return "FIN_WAIT1";
        case 5:  return "FIN_WAIT2";
        case 6:  return "TIME_WAIT";
        case 7:  return "CLOSE";
        case 8:  return "CLOSE_WAIT";
        case 9:  return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        default: return "?";
    }
}

static int
print_diag(const struct inet_diag_msg *diag, unsigned int size)
{
	if (size < NLMSG_LENGTH(sizeof(*diag))) {
		fputs("short response\n", stderr);
		return -1;
	}

	if (diag->idiag_family != AF_INET) {
		fprintf(stderr, "unexpected family %u\n", diag->idiag_family);
		return -1;
	}

	__u32 sk_mem_info[SK_MEMINFO_VARS] = {0};

	unsigned int rta_len = size - NLMSG_LENGTH(sizeof(*diag));
	size_t sz = 0;

	struct tcp_info tcp_info = {0};

	// вот тут бы match .... 
	for (struct rtattr *attr = (struct rtattr *) (diag + 1);
				RTA_OK(attr, rta_len); 
				attr = RTA_NEXT(attr, rta_len)) {

		switch (attr->rta_type) {
		case INET_DIAG_SKMEMINFO:
			if (!sz) {
				sz = RTA_PAYLOAD(attr);
				assert(sz == sizeof(sk_mem_info));
				memcpy(sk_mem_info, RTA_DATA(attr), sz);
			}
			break;
		case INET_DIAG_INFO:
			assert(RTA_PAYLOAD(attr) == sizeof(tcp_info));
			tcp_info = *(struct tcp_info *) RTA_DATA(attr);
			break;
		}
	}

	printf("-------------------------------\n");
	printf("inode = %d\n",diag->idiag_inode);
	printf("idiag_state = %u: %s\n", diag->idiag_state, state_str(diag->idiag_state));

    printf("SK_MEMINFO_RMEM_ALLOC\t%d\n", sk_mem_info[SK_MEMINFO_RMEM_ALLOC]);
    printf("SK_MEMINFO_RCVBUF\t%d\n", sk_mem_info[SK_MEMINFO_RCVBUF]);
    
	printf("SK_MEMINFO_WMEM_ALLOC\t%d\n", sk_mem_info[SK_MEMINFO_WMEM_ALLOC]);
    printf("SK_MEMINFO_SNDBUF\t%d\n", sk_mem_info[SK_MEMINFO_SNDBUF]);
   
    printf("SK_MEMINFO_FWD_ALLOC\t%d\n", sk_mem_info[SK_MEMINFO_FWD_ALLOC]);
	printf("SK_MEMINFO_WMEM_QUEUED\t%d\n", sk_mem_info[SK_MEMINFO_WMEM_QUEUED]);
   
	printf("SK_MEMINFO_OPTMEM\t%d\n", sk_mem_info[SK_MEMINFO_OPTMEM]);
	printf("SK_MEMINFO_BACKLOGS\t%d\n", sk_mem_info[SK_MEMINFO_BACKLOG]);
	printf("SK_MEMINFO_DROPS\t%d\n", sk_mem_info[SK_MEMINFO_DROPS]);
	return 0;
}


static int
receive_responses(int fd)
{
	long buf[8192 / sizeof(long)];
	struct sockaddr_nl nladdr;
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf)
	};
	int flags = 0;
	for (;;) {
		struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1
		};
		ssize_t ret = recvmsg(fd, &msg, flags);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("recvmsg");
			return -1;
		}
		if (ret == 0)
			return 0;
		if (nladdr.nl_family != AF_NETLINK) {
			fputs("!AF_NETLINK\n", stderr);
			return -1;
		}
		const struct nlmsghdr *h = (struct nlmsghdr *) buf;
		if (!NLMSG_OK(h, ret)) {
			fputs("!NLMSG_OK\n", stderr);
			return -1;
		}
		for (; NLMSG_OK(h, ret); h = NLMSG_NEXT(h, ret)) {
			if (h->nlmsg_type == NLMSG_DONE)
				return 0;
			if (h->nlmsg_type == NLMSG_ERROR) {
				const struct nlmsgerr *err = NLMSG_DATA(h);
				if (h->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
					fputs("NLMSG_ERROR\n", stderr);
				} else {
					errno = -err->error;
					perror("NLMSG_ERROR");
				}
				return -1;
			}
			if (h->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
				fprintf(stderr, "unexpected nlmsg_type %u\n",
						(unsigned) h->nlmsg_type);
				return -1;
			}
			if (print_diag(NLMSG_DATA(h), h->nlmsg_len))
				return -1;
		}
	}
}
int
main(void)
{
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
	if (fd < 0) {
		perror("socket");
		return 1;
	}
	int ret = send_query(fd) || receive_responses(fd);
	close(fd);
	return ret;
}
