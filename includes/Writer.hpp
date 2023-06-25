#ifndef WRITE_HPP
# define WRITE_HPP

# include <string>
# include <map>
# include <fstream>
# include <iostream>
# include <cstddef>
# include "Buf.hpp"

typedef std::map<std::string const, std::string const> MimeMap;
std::string get_mime_type(const std::string& file_path, MimeMap const& mimes);

size_t rem_stream_size(std::istream & in);

class Writer {
public:

	Writer();
// 	Writer(Writer const& other); // default segfaults ???
	void reset(); // same state as just after calling ctor

	/* Prepare Header */
	// Note : `set_status` must be called BEFORE any call to `add_header_field`
	// (between resets)
	void set_status(int code);
	void add_header_field(std::string const& key, std::string const& val);

	/* Prepare body */

	// Reset body. Use `consume` as body. 
	// the state of `consume` after the call is undefined.
	// impl: swaps `consume` with the current `_body`
	void use_as_body(std::string & consume);

	static const size_t READ_SIZE = 2048;
	// Use content of file as body
	// Return `true` if ok `false` if reading error 
// 	bool read_body_from_file(int fd);
	bool read_body_from_stream(std::istream & f);

	// Asks to attempt to stream the content of `f`
	// as the body.
	// Used to avoid allocating huge bodies.
	void stream_file_to_body(std::ifstream & f);

	// Append `s` to body
	void body_append(std::string const& s);
	void body_append(char * s, size_t n);

	/* Size */
// 	size_t body_size() const;

	/* Write */
	static const int 
		READ_ERROR = -2, WRITE_ERROR = -1, PARTIAL_WRITE = 0, OK_FINISHED = 1;
	static const size_t MAX_WRITE_SIZE = 8192;
	// Write some data (as much as was sent by a single `send` call) 
	// to `fd_out` (socket). use `send_opt` as last arg of `send`.
	// 
	// Returns values may be:
	// 	Writer::READ_ERROR when streaming from an ifstream
	// 	Writer::WRITE_ERROR
	// 	Writer::PARTIAL_WRITE
	// 	Writer::OK_FINISHED
	int write_some(int fd_out, int send_opt = 0);

private:
	size_t _pos;
	std::string _header;
	std::string _body; 

	bool _streaming;
	Buf _rdbuf;
	std::ifstream *_body_file;
};

#endif
