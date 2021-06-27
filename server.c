#include "common.h"

void run_server(const char*);

int main(int argc, char** argv) {
	const char* payload;
	if (argc >= 2) {
		payload = argv[1];
	} else {
		payload = "Hello from " __FILE__;
	}

	run_server(payload);
}

void run_server(const char* payload) {
	int sockfd;
	struct sockaddr_in6 sin6;
	char buffer[MAX_LINE];
	char addr[INET6_ADDRSTRLEN];
	unsigned int len = sizeof(sin6), n;

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0)
		fatal_error("socket creation failed");

	memset(&sin6, 0, len);

	sin6.sin6_port = htons(PORT);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;

	if (bind(sockfd, (const struct sockaddr*)&sin6, len))
		fatal_error("bind failed");

	struct iovec v[] = {{buffer, sizeof(buffer) / sizeof(buffer[0])}};
	struct msghdr msg = {&sin6, len, v, sizeof(v) / sizeof(v[0]), NULL, 0, 0};
	n = recvmsg(sockfd, &msg, MSG_WAITALL);
	buffer[n] = '\0';
	inet_ntop(sin6.sin6_family, &sin6.sin6_addr, addr, len);

	printf("Client@[%s] : %s\n", addr, buffer);

	v[0].iov_base = (char*)payload;
	v[0].iov_len = strlen(payload);
	sendmsg(sockfd, &msg, MSG_CONFIRM);

	if (close(sockfd))
		fatal_error("failed to close socket");
}