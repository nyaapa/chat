#include <liburing.h>
#include "common.h"

void run_client_load(const char*, int);

int main(int argc, char** argv) {
	const char* payload;
	if (argc >= 3) {
		payload = argv[2];
	} else {
		payload = "Hello from " __FILE__;
	}

	if (argc < 2)
		fatal_error("Please pass it count");

	run_client_load(payload, atoi(argv[1]));
}

void run_client_load(const char* payload, int repeat) {
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

	struct io_uring ring;
	if (io_uring_queue_init(URING_ENTRIES, &ring, 0) < 0)
		fatal_error("io_uring_queue_init failed");

	struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);

	struct io_uring_entry entry_send = {sockfd, IORING_OP_SENDMSG, IOSQE_IO_LINK, (unsigned long long)&msg, &io_uring_print_sendmsg};
	io_uring_prep_sendmsg(sqe, sockfd, &msg, 0);
	io_uring_sqe_set_data(sqe, &entry_send);

	if (io_uring_submit(&ring) < 0)
		fatal_error("io_uring_submit failed");

	struct io_uring_cqe* cqe;
	if (io_uring_wait_cqe(&ring, &cqe) < 0)
		fatal_error("io_uring_wait_cqe failed");

	struct io_uring_entry* entry = (struct io_uring_entry*)io_uring_cqe_get_data(cqe);

	entry->handler(entry);

	io_uring_cqe_seen(&ring, cqe);

	while (--repeat >= 0) {
		// yeah, overrides
		struct iovec v_recv[] = {{buffer, sizeof(buffer) / sizeof(buffer[0])}};
		struct msghdr msg_recv = {&sin6, len, v_recv, sizeof(v_recv) / sizeof(v_recv[0]), NULL, 0, 0};
		struct io_uring_entry entry_recv = {sockfd, IORING_OP_RECVMSG, 0, (unsigned long long)&msg_recv, &io_uring_nop};
		for (int i = 0; i < BATCH_SIZE; ++i) {
			sqe = io_uring_get_sqe(&ring);
			io_uring_prep_recvmsg(sqe, sockfd, &msg_recv, 0);
			io_uring_sqe_set_data(sqe, &entry_recv);
		}

		int submitted = io_uring_submit(&ring);
		if (submitted != BATCH_SIZE) {
			printf("Submitted just %i/%i\n", submitted, BATCH_SIZE);
		}

		struct io_uring_cqe* cqes[BATCH_SIZE];
		for (int read = 0; read < BATCH_SIZE;) {
			int completions = io_uring_peek_batch_cqe(&ring, cqes, BATCH_SIZE - read);
			if (completions == 0) {
				continue;
			}

			for (int j = 0; j < completions; j++) {
				if (cqes[j]->res < 0) {
					printf("Async task failed: %s\n", strerror(-cqes[j]->res));
				} else {
					struct io_uring_entry* entry = (struct io_uring_entry*)io_uring_cqe_get_data(cqes[j]);
					entry->handler(entry);
				}

				io_uring_cqe_seen(&ring, cqes[j]);
			}

			read += completions;
		}
	}

	struct io_uring_entry entry_close = {sockfd, IORING_OP_CLOSE, IOSQE_IO_LINK, 0, &io_uring_print_closemsg};
	sqe = io_uring_get_sqe(&ring);
	io_uring_prep_close(sqe, sockfd);
	io_uring_sqe_set_data(sqe, &entry_close);

	if (io_uring_submit(&ring) < 0)
		fatal_error("io_uring_submit failed");

	if (io_uring_wait_cqe(&ring, &cqe) < 0)
		fatal_error("io_uring_wait_cqe failed");

	entry = (struct io_uring_entry*)io_uring_cqe_get_data(cqe);

	entry->handler(entry);

	io_uring_cqe_seen(&ring, cqe);
}