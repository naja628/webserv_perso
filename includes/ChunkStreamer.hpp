#ifndef CHUNKSTREAMER_HPP
# define CHUNKSTREAMER_HPP

# include <cstddef>
# include "Buf.hpp"
# include "CgiHandler.hpp"

class ChunkStreamer {

public:
	enum Status 
	{ START = 0, CHUNK_PREFIX = 0, CHUNK_PAYLOAD, CRLF, TRAILER, DONE };

	ChunkStreamer(int fd = -1); // TODO handle write errors

	Status status() const;

	void setfd(int fd);

	void set_max_size(size_t sz);
	void set_chunk_size(size_t sz);

	Status read_some_chunked(Buf & buf, CgiHandler &cgi);
	Status read_some_single(Buf & buf, CgiHandler &cgi);

private:
	int    _wrfd;
	Status _status;
	size_t _rem_chunk_size;

	size_t _max_allowed_size;
	size_t _min_expected_size;

	bool _stream_chunk_payload(Buf & buf, CgiHandler &cgi);
	bool _get_chunk_size(Buf & buf);
	bool _expect_crlf(Buf & buf);
	bool _discard_trailer(Buf & buf);

};

#endif
