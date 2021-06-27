#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <netinet/in.h>

const int PORT = 4242;
const int MAX_LINE = 1024;

void fatal_error(const char* desc) {
	perror(desc);
	exit(1);
}