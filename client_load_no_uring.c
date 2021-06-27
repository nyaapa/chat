#include "common.h"

void run_client_load_no_ring(const char*, int);

int main(int argc, char** argv) {
	const char* payload;
	if (argc >= 3) {
		payload = argv[2];
	} else {
		payload = "Hello from " __FILE__;
	}

	if (argc < 2)
		fatal_error("Please pass it count");

	run_client_load_no_ring(payload, atoi(argv[1]));
}

void run_client_load_no_ring(const char* payload, int repeat) {
	int sockfd;
	char buffer[MAX_LINE];
	struct sockaddr_in6 sin6;
	unsigned int len = sizeof(sin6);

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0)
		fatal_error("socket creation failed");

	memset(&sin6, 0, len);

	sin6.sin6_port = htons(PORT);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;

	struct iovec v[] = {{(char*)payload, strlen(payload)}};
	struct msghdr msg = {&sin6, len, v, sizeof(v) / sizeof(v[0]), NULL, 0, 0};

	sendmsg(sockfd, &msg, MSG_CONFIRM);

	// know that it's not fully fair
	while (--repeat >= 0) {
		for (int i = 0; i < BATCH_SIZE; ++i) {
			v[0].iov_base = buffer;
			v[0].iov_len = sizeof(buffer) / sizeof(buffer[0]);
			recvmsg(sockfd, &msg, MSG_WAITALL);
		}
	}

	if (close(sockfd))
		fatal_error("failed to close socket");
}