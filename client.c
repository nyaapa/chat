#include "common.h"

void run_client(const char*);

int main(int argc, char** argv) {
	const char* payload;
	if (argc >= 2) {
		payload = argv[1];
	} else {
		payload = "Hello from " __FILE__;
	}

	run_client(payload);
}

void run_client(const char* payload) {
	int sockfd;
	char buffer[MAX_LINE];
	char addr[INET6_ADDRSTRLEN];
	struct sockaddr_in6 sin6;
	unsigned int len = sizeof(sin6), n;

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&sin6, 0, len);

	sin6.sin6_port = htons(PORT);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;

	struct iovec v[] = {{(char*)payload, strlen(payload)}};
	struct msghdr msg = {&sin6, len, v, sizeof(v) / sizeof(v[0]), NULL, 0, 0};

	sendmsg(sockfd, &msg, MSG_CONFIRM);

	v[0].iov_base = buffer;
	;
	v[0].iov_len = sizeof(buffer) / sizeof(buffer[0]);
	n = recvmsg(sockfd, &msg, MSG_WAITALL);
	buffer[n] = '\0';
	inet_ntop(sin6.sin6_family, &sin6.sin6_addr, addr, len);
	printf("Server@[%s] : %s\n", addr, buffer);

	if (close(sockfd)) {
		perror("failed to close socket");
		exit(EXIT_FAILURE);
	}
}