#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <algorithm> // just for `min`
#include "nat_utils.hpp"
#include "Writer.hpp"
#include "HttpError.hpp"

#include <iostream>

std::string get_mime_type(const std::string& file_path, MimeMap const& mimes)
{
        const std::string::size_type dott_indx = file_path.find_last_of('.');
        if (dott_indx != std::string::npos)
        {
			MimeMap::const_iterator target = mimes.find(file_path.substr(dott_indx + 1));
			if (target != mimes.end())
				return target->second;
        }

        return "application/octet-stream";
}

size_t rem_stream_size(std::istream & in) {
	if (!in)
		return 0;
	std::streampos cur, end;
	cur = in.tellg();
	in.seekg(0, std::ios_base::end);
	end = in.tellg();
	in.seekg(cur);
	std::cerr << "file size" << end - cur << "\n";

	return end - cur;
}

Writer::Writer() : _pos(0), _streaming(false), _body_file(NULL) {}

// Writer::Writer(Writer const& other) {
// 	_pos = other._pos;
// 	_header = other._header;
// 	_body = other._body;
// 
// 	_streaming = other._streaming;
// 	_rdbuf = other._rdbuf;
// 	_body_file = other._body_file;
// }

void Writer::reset() {
	using std::swap;

	_pos = 0;
	_header = "";
	_body = "";
// 	swap(_header, std::string());
// 	swap(_body, std::string());

	_streaming = false;
	_rdbuf = Buf();
	_body_file = NULL;
}

int Writer::write_some(int fd_out, int send_opt) {
	if (_pos == 0) { // first write
		if (_header.find("\r\ncontent-length:") == std::string::npos) {
			add_header_field(
					"content-length", 
					strof(_body_file ? rem_stream_size(*_body_file) : _body.size() ));
		}
		_header += "\r\n";
		if (!_body_file)
			_header += _body;
	}

	ssize_t wrbytes;
	if (!_streaming) {
		wrbytes = send(
				fd_out, _header.data() + _pos,
				std::min((size_t) MAX_WRITE_SIZE, _header.size() - _pos), send_opt);
		if (wrbytes < 0) {
			return WRITE_ERROR;
		}
		_pos += wrbytes;
		if (_pos == _header.size()) {
			if (_body_file) {
				_streaming = true;
				return PARTIAL_WRITE;
			} else {
				return OK_FINISHED;
			}
		} else {
			return PARTIAL_WRITE;
		}
	} else {
		if ( !_body_file->is_open() )
			return READ_ERROR;
		_rdbuf.read_more(*_body_file);
		if (_body_file->fail() && !_body_file->eof())
			return READ_ERROR;
		wrbytes = send(fd_out, _rdbuf.pos, _rdbuf.pending_bytes(), send_opt);
		if (wrbytes < 0) {
			return WRITE_ERROR;
		}
		_rdbuf.pos += wrbytes;
		return _body_file->eof() ? OK_FINISHED : PARTIAL_WRITE;
	}
}

void Writer::set_status(int code) {
	std::ostringstream ss;
	_header += "HTTP/1.1 ";
	_header += strof(code) + " ";
	_header += HttpError(code).reason();
	_header += "\r\n";
}

void Writer::add_header_field(std::string const& key, std::string const& val) {
	_header += key + ": ";
	_header += val + "\r\n";
}

// bool Writer::read_body_from_file(int fd) {
// 	char buf[READ_SIZE];
// 	ssize_t rdbytes;
// 	while ( (rdbytes = read(fd, buf, READ_SIZE)) > 0 )
// 		_body.append(buf, rdbytes);
// 	return (rdbytes < 0 ? false : true ); // false -> reading error
// }

// bool Writer::read_body_from_stream(std::istream & f) {
// 	char buf[READ_SIZE];
// 	while (f.read(buf, READ_SIZE))
// 		_body.append(buf, f.gcount());
// 	return (f.fail() && !f.eof() ? false : true ); // false -> reading error
// }

bool Writer::read_body_from_stream(std::istream & f) {
	char buf[READ_SIZE];
	while (true) {
		f.read(buf, READ_SIZE);
		_body.append(buf, f.gcount());
		if (f.eof())
			return true;
		if (f.fail())
			return false;
	}
}

void Writer::stream_file_to_body(std::ifstream & f) {
	_body_file = &f;
}

void Writer::body_append(std::string const& s) {
	_body += s;
}

void Writer::body_append(char * s, size_t n) {
	_body.append(s, n);
}

void Writer::use_as_body(std::string & consume) {
	_body.swap(consume);
}

// size_t Writer::body_size() const {
// 	return _body.size();
// }
