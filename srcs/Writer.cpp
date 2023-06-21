#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <algorithm> // just for `min`
#include "nat_utils.hpp"
#include "Writer.hpp"
#include "HttpError.hpp"

#include <iostream>
Writer::Writer() : _pos(0) {}

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

void Writer::reset() {
	using std::swap;
	Writer tmp;
	swap(_header, tmp._header);
	swap(_body, tmp._body);
	_pos = 0;
}

// TODO for chunks
// int Writer::_write_once(int fd_out, int send_opt, str_) {
// 	ssize_t wrbytes = send(
// 			fd_out, _header.data() + _pos,
// 		   	std::min((size_t) 8192, _header.size() - _pos), send_opt);
// // 	(void) send_opt;
// // 	ssize_t wrbytes = send( fd_out, _header.data() + _pos, _header.size() - _pos, SOCK_NONBLOCK);
// 	std::cerr << "post write\n";
// 	if (wrbytes < 0) {
// 		return WRITE_ERROR;
// 	}
// 	std::cerr << "fd = " << fd_out << ", wrbytes = " << wrbytes << "\n";
// 
// // 	std::cerr << "just wrote:\n";
// // 	write(2, _header.data() + _pos, std::min(wrbytes, (ssize_t) 20));
// 	_pos += wrbytes;
// // 	std::cerr << "\nbufpos(after):" << _pos << "\n";
// 	return (_pos == _header.size() ? OK_FINISHED : PARTIAL_WRITE);
// }
// 
// int Writer::write_some(int fd_out, int send_opt) {
// 	if (_pos == 0) { // first write
// 		if (_header.find("\r\ncontent-length:") == std::string::npos)
// 			add_header_field("content-length", strof(body_size()));
// 		_header += "\r\n";
// 	}
// 	if ( _chunked_mode ) {
// 
// 	} else {
// 		_header += _body;
// 	}
// 	return _write_once;
// }

int Writer::write_some(int fd_out, int send_opt) {
	if (_pos == 0) { // first write
		if (_header.find("\r\ncontent-length:") == std::string::npos)
			add_header_field("content-length", strof(body_size()));
		_header += "\r\n";
		_header += _body;
	}

	ssize_t wrbytes = send(
			fd_out, _header.data() + _pos,
		   	std::min((size_t) MAX_WRITE_SIZE, _header.size() - _pos), send_opt);
// 	std::cerr << "post write\n";
	if (wrbytes < 0) {
		return WRITE_ERROR;
	}
// 	std::cerr << "fd = " << fd_out << ", wrbytes = " << wrbytes << "\n";

	_pos += wrbytes;
	return (_pos == _header.size() ? OK_FINISHED : PARTIAL_WRITE);
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

bool Writer::read_body_from_file(int fd) {
	char buf[READ_SIZE];
	ssize_t rdbytes;
	while ( (rdbytes = read(fd, buf, READ_SIZE)) > 0 )
		_body.append(buf, rdbytes);
	return (rdbytes < 0 ? false : true ); // false -> reading error
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

size_t Writer::body_size() const {
	return _body.size();
}
