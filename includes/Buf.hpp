#ifndef BUF_HPP
# define BUF_HPP

# include <cstring>
# include <iostream>

struct Buf {
	static const size_t BUFSIZE = 4096;
	char start[BUFSIZE];
	char * pos;
	char * end;

	Buf();

	// Canonical form:
	Buf(Buf const& other);
	Buf & operator=(Buf const& other);
	
	size_t read_more(int sockfd, int sockopt = 0);
	size_t read_more(std::istream & in);

	bool full() const;
	bool empty() const;
	
	size_t pending_bytes() const;
	size_t max_size() const;

	bool passover(char const* start);
};
#undef BUFISZE

#endif
