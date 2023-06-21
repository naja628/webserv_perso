#ifndef WRITE_HPP
# define WRITE_HPP

#include <string>
#include <map>

typedef std::map<std::string const, std::string const> MimeMap;
std::string get_mime_type(const std::string& file_path, MimeMap const& mimes);

class Writer {
public:
//  TODO for chunks
// 	static const int 
// 		START = 0, SEND_HEADER = 1, SEND_BODY = 2, SEND_CHUNK = 3, DONE = 4;
// 	static const size_t READ_BUFFER_SIZE = 2048;
// 	static const size_t CHUNK_BUFFER_SIZE = 2048;

	Writer();
	void reset(); // same state as juste after calling ctor

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
	bool read_body_from_file(int fd);

	// Append `s` to body
	void body_append(std::string const& s);
	void body_append(char * s, size_t n);

	/* Size */
	size_t body_size() const;

	/* Write */
	static const int WRITE_ERROR = -1, PARTIAL_WRITE = 0, OK_FINISHED = 1;
	static const size_t MAX_WRITE_SIZE = 2048;
	// Write some data (as much as was sent by a single `send` call) 
	// to `fd_out` (socket). use `send_opt` as last arg of `send`.
	// 
	// Returns values may be:
	// 	Writer::WRITE_ERROR
	// 	Writer::PARTIAL_WRITE
	// 	Writer::OK_FINISHED
	int write_some(int fd_out, int send_opt = 0);

private:
	std::string _header;
	std::string _body; 
	size_t _pos;

};

#endif
