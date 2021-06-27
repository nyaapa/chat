#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <asm-generic/mman-common.h>
#include <linux/io_uring.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#define mem_barrier() __asm__ __volatile__("" ::: "memory")

const int PORT = 4242;
const int MAX_LINE = 1024;
const int URING_ENTRIES = 2048;
const int BATCH_SIZE = 1024;

struct io_uring_sq_ring {
	unsigned* head;
	unsigned* tail;
	unsigned* ring_mask;
	unsigned* ring_entries;
	unsigned* flags;
	unsigned* array;
};

struct io_uring_cq_ring {
	unsigned* head;
	unsigned* tail;
	unsigned* ring_mask;
	unsigned* ring_entries;
	struct io_uring_cqe* cqes;
};

struct io_uring_l {
	int fd;
	struct io_uring_sq_ring sq;
	struct io_uring_cq_ring cq;
	struct io_uring_sqe* sqes;
};

struct io_uring_entry {
	int fd;
	int op;
	unsigned char flags;
	unsigned long long addr;
	void (*handler)(struct io_uring_entry*);
};

void fatal_error(const char* desc) {
	perror(desc);
	exit(EXIT_FAILURE);
}

int setup_io_uring(struct io_uring_l* ring) {
	struct io_uring_params p;
	memset(&p, 0, sizeof(p));

	if ((ring->fd = (int)syscall(__NR_io_uring_setup, URING_ENTRIES, &p)) <= 0) {
		perror("failed to setup io_uring");
		return EXIT_FAILURE;
	}

	int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
	int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

	if (cring_sz > sring_sz)
		sring_sz = cring_sz;
	cring_sz = sring_sz;

	void *sq_ptr, *cq_ptr;
	sq_ptr = (unsigned*)mmap(0, sring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring->fd, IORING_OFF_SQ_RING);
	if (sq_ptr == MAP_FAILED) {
		perror("io_uring mmap sq_ring failed");
		return EXIT_FAILURE;
	}

	cq_ptr = sq_ptr;

	ring->sq.head = sq_ptr + p.sq_off.head;
	ring->sq.tail = sq_ptr + p.sq_off.tail;
	ring->sq.ring_mask = sq_ptr + p.sq_off.ring_mask;
	ring->sq.ring_entries = sq_ptr + p.sq_off.ring_entries;
	ring->sq.flags = sq_ptr + p.sq_off.flags;
	ring->sq.array = sq_ptr + p.sq_off.array;

	ring->sqes =
	    (struct io_uring_sqe*)mmap(0, p.sq_entries * sizeof(struct io_uring_sqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring->fd, IORING_OFF_SQES);
	if (ring->sqes == MAP_FAILED) {
		perror("io_uring mmap sqes failed");
		return EXIT_FAILURE;
	}

	ring->cq.head = cq_ptr + p.cq_off.head;
	ring->cq.tail = cq_ptr + p.cq_off.tail;
	ring->cq.ring_mask = cq_ptr + p.cq_off.ring_mask;
	ring->cq.ring_entries = cq_ptr + p.cq_off.ring_entries;
	ring->cq.cqes = (struct io_uring_cqe*)(cq_ptr + p.cq_off.cqes);

	return EXIT_SUCCESS;
}

int io_uring_entry_and_read(struct io_uring_l* ring, int* count) {
	struct io_uring_cq_ring* cring = &ring->cq;

	if (*cring->head == *cring->tail && syscall(__NR_io_uring_enter, ring->fd, 0, 1, IORING_ENTER_GETEVENTS, NULL) < 0) {
		perror("io_uring_enter failed");
		return EXIT_FAILURE;
	}
	struct io_uring_cqe* cqe;

	unsigned head = *cring->head;

	mem_barrier();

	while (head != *cring->tail) {
		cqe = &cring->cqes[head & *ring->cq.ring_mask];

		if (cqe->res < 0)
			fprintf(stderr, "Error: %s\n", strerror(abs(cqe->res)));

		struct io_uring_entry* entry = (struct io_uring_entry*)cqe->user_data;
		entry->handler(entry);

		++head;
		++(*count);

		mem_barrier();
	}

	*cring->head = head;

	mem_barrier();

	return EXIT_SUCCESS;
}

void io_uring_add(struct io_uring_entry* entry, struct io_uring_l* ring) {
	struct io_uring_sq_ring* sring = &ring->sq;
	unsigned tail = *sring->tail;
	unsigned index = tail & *sring->ring_mask;

	struct io_uring_sqe* sqe = &ring->sqes[index];
	sqe->fd = entry->fd;
	sqe->flags = entry->flags;
	sqe->opcode = entry->op;
	sqe->addr = entry->addr;
	sqe->len = !!sqe->addr;
	sqe->off = 0;
	sqe->user_data = (long long)entry;
	sqe->__pad2[0] = sqe->__pad2[1] = sqe->__pad2[2] = 0;
	sring->array[index] = index;

	mem_barrier();
	*sring->tail = tail + 1;
	mem_barrier();
}

int io_uring_enter(struct io_uring_l* ring, int count) {
	int ret = syscall(__NR_io_uring_enter, ring->fd, count, count, IORING_ENTER_GETEVENTS, NULL);
	if (ret <= 0) {
		perror("io_uring_enter failed");
		return ret;
	}

	return ret;
}

void io_uring_print_sendmsg(struct io_uring_entry* entry) {
	struct msghdr* msg = (struct msghdr*)entry->addr;

	for (size_t i = 0; i < msg->msg_iovlen; i++) {
		char* buf = (char*)msg->msg_iov[i].iov_base;
		int len = msg->msg_iov[i].iov_len;
		printf("Sent -> \"%.*s\"\n", len, buf);
	}
}

void io_uring_nop(struct io_uring_entry*) {}

void io_uring_print_recvmsg(struct io_uring_entry* entry) {
	struct msghdr* msg = (struct msghdr*)entry->addr;

	char addr[INET6_ADDRSTRLEN];
	struct sockaddr_in6* sin6 = msg->msg_name;

	inet_ntop(sin6->sin6_family, &sin6->sin6_addr, addr, msg->msg_namelen);

	for (size_t i = 0; i < msg->msg_iovlen; i++) {
		char* buf = (char*)msg->msg_iov[i].iov_base;
		int len = msg->msg_iov[i].iov_len;
		printf("Recv from [%s]:%i -> \"%.*s\"\n", addr, sin6->sin6_port, len, buf);
	}
}

void io_uring_print_closemsg(struct io_uring_entry* entry) {
	printf("Closed fd -> %i\n", entry->fd);
}