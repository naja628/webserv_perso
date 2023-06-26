#include <iostream>
#include <cstddef>
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm> // min

#include <unistd.h>

#include "HttpError.hpp"
#include "Buf.hpp"

#include "ChunkStreamer.hpp"
#include "CgiHandler.hpp"

#define INSUFFICIENT_DATA 0
#define FUN_OK 1

ChunkStreamer::ChunkStreamer(int fd)
	: _wrfd(fd), _status(CHUNK_PREFIX), _rem_chunk_size(0),
	  _max_allowed_size(0), _min_expected_size(0)
{}

ChunkStreamer::Status ChunkStreamer::status() const { return _status; }

void ChunkStreamer::setfd(int fd) { _wrfd = fd; }
void ChunkStreamer::set_chunk_size(size_t chunk_size) {
	_rem_chunk_size = chunk_size;
}
void ChunkStreamer::set_max_size(size_t max_size) { 
	_max_allowed_size = max_size;
}

ChunkStreamer::Status ChunkStreamer::read_some_chunked(Buf & buf, CgiHandler &cgi) {
	while (1) {
		if (_status == TRAILER) {
			return _discard_trailer(buf) ? DONE : TRAILER;
		}
		switch (_status) {
			case CHUNK_PREFIX:
				if (_get_chunk_size(buf) == INSUFFICIENT_DATA)
					return (_status = CHUNK_PREFIX);
				_min_expected_size += _rem_chunk_size;

				if (_rem_chunk_size == 0) {
					_status = TRAILER;
					break;
				}
				// fallthrough
			case CHUNK_PAYLOAD:
				if (_max_allowed_size && _min_expected_size > _max_allowed_size)
					throw HttpError(413);
				_stream_chunk_payload(buf, cgi);
				if (_rem_chunk_size > 0)
					return (_status = CHUNK_PAYLOAD);
				// fallthrough
			case CRLF:
				if (_expect_crlf(buf) == INSUFFICIENT_DATA)
					return (_status = CRLF);
				_status = TRAILER;
				break;

				// impossible path
			case TRAILER:
			default:
				return (_status = DONE);
		}
	}
}

ChunkStreamer::Status ChunkStreamer::read_some_single(Buf & buf, CgiHandler &cgi) {
	_stream_chunk_payload(buf, cgi);
	if (_rem_chunk_size > 0)
		return (_status = CHUNK_PAYLOAD);
	return (_status = DONE);
}

bool ChunkStreamer::_stream_chunk_payload(Buf & buf, CgiHandler &cgi) {
	ssize_t wrbytes;
	wrbytes = cgi.bodyInFile(buf, _rem_chunk_size);
	buf.pos += wrbytes;
	_rem_chunk_size -= wrbytes;
	return (_rem_chunk_size == 0? FUN_OK : INSUFFICIENT_DATA);
}

bool ChunkStreamer::_get_chunk_size(Buf & buf) {
	char const* crlf;

	// find end of current line
	if ( !(crlf = strstr(buf.pos, "\r\n")) )
	{
		if ( buf.full() )
			throw HttpError(500);
		return INSUFFICIENT_DATA;
	}

	if (crlf == buf.pos) // error empty chunk-size
		throw HttpError(400);

	// put the `chunk-size [chunk-extension]` data into a string
	// to init a stringstream with
	std::string hex_repr;
	hex_repr.append(buf.pos, crlf - buf.pos);
	// note: somewhat wasteful as we also load the extension into the string

	// use stringstream to convert chunk_size
	std::istringstream ss(hex_repr);
	ss >> std::hex;
	ss >> _rem_chunk_size;
	if ( !(ss.peek() == ';' || ss.eof()) )
		throw HttpError(400);

	buf.pos = (char *) crlf + 2;
	return FUN_OK;
}

bool ChunkStreamer::_expect_crlf(Buf & buf) {
	if (buf.pending_bytes() < 2)
		return INSUFFICIENT_DATA;

	if (buf.passover("\r\n"))
		return FUN_OK;
	else 
		throw HttpError(400);
}

bool ChunkStreamer::_discard_trailer(Buf & buf) {
	// `buf.pos` is always at the start of a (possibly empty) trailer line.

	while (1)
	{
		char const* crlf = strstr(buf.pos, "\r\n");
		if (!crlf)
			return INSUFFICIENT_DATA;

		if (buf.pos == crlf) {
			buf.pos += 2;
			return FUN_OK;
		} else {
			buf.pos = (char *) crlf + 2;
		}
	}
}

#undef INSUFFICIENT_DATA
#undef FUN_OK
