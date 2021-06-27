#include "common.h"

void run_server_load(const char*, int);

int main(int argc, char** argv) {
	const char* payload;
	if (argc >= 3) {
		payload = argv[2];
	} else {
		payload = "Hello from " __FILE__;
	}

	if (argc < 2)
		fatal_error("Please pass it count");

	run_server_load(payload, atoi(argv[1]));
}

void run_server_load(const char* payload, int repeat) {
	int sockfd;
	struct sockaddr_in6 sin6;
	char buffer[MAX_LINE];
	unsigned int len = sizeof(sin6);

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0)
		fatal_error("socket creation failed");

	memset(&sin6, 0, len);

	sin6.sin6_port = htons(PORT);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;

	if (bind(sockfd, (const struct sockaddr*)&sin6, len))
		fatal_error("bind failed");

	struct io_uring_l ring;
	memset(&ring, 0, sizeof(ring));
	if (setup_io_uring(&ring))
		exit(EXIT_FAILURE);

	struct iovec v_recv[] = {{buffer, sizeof(buffer) / sizeof(buffer[0])}};
	struct msghdr msg_recv = {&sin6, len, v_recv, sizeof(v_recv) / sizeof(v_recv[0]), NULL, 0, 0};
	struct io_uring_entry entry_recv = {sockfd, IORING_OP_RECVMSG, IOSQE_IO_LINK, (unsigned long long)&msg_recv, &io_uring_print_recvmsg};
	io_uring_add(&entry_recv, &ring);

	if (io_uring_enter(&ring, 1) != 1)
		exit(EXIT_FAILURE);

	int count;
	if (io_uring_entry_and_read(&ring, &count))
		exit(EXIT_FAILURE);

	while (--repeat >= 0) {
		count = 0;
		// even though msghdr can do it itself -> let's just measure on top of it
		struct iovec v_send[] = {{(char*)payload, strlen(payload)}};
		struct msghdr msg_send = {&sin6, len, v_send, sizeof(v_send) / sizeof(v_send[0]), NULL, 0, 0};
		struct io_uring_entry entry_send = {sockfd, IORING_OP_SENDMSG, 0, (unsigned long long)&msg_send, &io_uring_nop};
		for (int i = 0; i < BATCH_SIZE; ++i) {
			io_uring_add(&entry_send, &ring);
		}

		if (io_uring_enter(&ring, BATCH_SIZE) != BATCH_SIZE)
			exit(EXIT_FAILURE);

		while (count < BATCH_SIZE) {
			if (io_uring_entry_and_read(&ring, &count))
				exit(EXIT_FAILURE);
		}
	}

	struct io_uring_entry entry_close = {sockfd, IORING_OP_CLOSE, IOSQE_IO_LINK, 0, &io_uring_print_closemsg};
	io_uring_add(&entry_close, &ring);

	if (io_uring_enter(&ring, 1) != 1)
		exit(EXIT_FAILURE);

	if (io_uring_entry_and_read(&ring, &count))
		exit(EXIT_FAILURE);
}