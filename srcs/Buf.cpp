#include <cstddef>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>

#include "Buf.hpp"

Buf::Buf() : pos(start), end(start) {}

// Canonical form:
Buf::Buf(Buf const& other) {
	pos = start + (other.pos - other.start);
	end = start + (other.end - other.start);
	memcpy(pos, other.pos, (end - pos) + 1 /* null byte */ );
}

Buf & Buf::operator=(Buf const& other) {
	pos = start + (other.pos - other.start);
	end = start + (other.end - other.start);
	memcpy(pos, other.pos, (end - pos) + 1 /* null byte */ );
	return *this;
}

size_t Buf::read_more(int sockfd, int sockopt) {
	size_t remsz = end - pos;
	memmove(start, pos, remsz);
	end = start + remsz;
	pos = start;

	ssize_t rdbytes = recv(sockfd, end, BUFSIZE - 1 - (end - start), sockopt);
	if (rdbytes < 0)
		return rdbytes;

	end += rdbytes;
	*end = '\0'; // null byte (so we can use `strstr`)
	return rdbytes;
}

size_t Buf::read_more(std::istream & in) {
	if (in.fail())
		return -1;

	size_t remsz = end - pos;
	memmove(start, pos, remsz);
	end = start + remsz;
	pos = start;

	in.read(end, BUFSIZE - 1 - (end - start));

	end += in.gcount();
	*end = '\0';
	return in.gcount();
}

bool Buf::full() const {
	return (pos == start) && (end == start + BUFSIZE - 1);
}

bool Buf::empty() const {
	return pos == end;
}

size_t Buf::pending_bytes() const {
	return end - pos;
}

size_t Buf::max_size() const {
	return BUFSIZE - 1;
}

bool Buf::passover(char const* start) {
	// if !start -> UB
	char * save_pos = pos;
	while (1) {
		if (*start == '\0')
			return true;
		if ( *(start++) != *(pos++) ) {
			pos = save_pos;
			return false;
		}
	}
}
