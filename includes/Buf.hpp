#ifndef BUF_HPP
# define BUF_HPP

# include <unistd.h>
# include <cstring>

#define BUFSIZE 4096
// #define BUFSIZE 64
struct Buf {
	char start[BUFSIZE];
	char * pos;
	char * end;

	Buf();

	// Canonical form:
	Buf(Buf const& other);
	Buf & operator=(Buf const& other);
	
	size_t read_more(int sockfd); // TODO maybe pass recv options

	bool full() const;
	bool empty() const;
	
	size_t pending_bytes() const;
	size_t max_size() const;

	bool passover(char const* start);
};
#undef BUFISZE

#endif
