#include <string>
#include <map>
#include "HttpError.hpp"
#include "nat_utils.hpp"
#include "HttpParser.hpp"

/* BufAdaptor */
BufAdaptor::BufAdaptor(Buf & buf) : _buf(buf) {}

char BufAdaptor::get() {
	return _buf.empty() ? NOCHAR : *(_buf.pos++);
}

char BufAdaptor::peek() const {
	return _buf.empty() ? NOCHAR : *(_buf.pos);
}

int BufAdaptor::count_lines(char const* nl) const {
	int count = 0;
	char const* crlf;
	char const* rdpos = _buf.pos;
	while ( (crlf = strstr(rdpos, nl)) ) {
		++count;
		rdpos = crlf + strlen(nl);
	}
	return count;
}

bool BufAdaptor::full() const {
	return _buf.full();
}

bool BufAdaptor::empty() const {
	return _buf.empty();
}

bool BufAdaptor::passover(char const* to_match) {
	return _buf.passover(to_match);
}
/* BUFFER MANIPULATION UTILS */

// read the contents of `buf` until the first character in `charset` 
// into the returned string.
// `charset` SHALL be null-terminated.
static std::string untilset(BufType & buf, char const* charset) {
	std::string res;
	while ( !member(buf.peek(), charset) )
		res.push_back( buf.get() );
	return res;
}

// if `buf` starts with `match`:
//     move read position just after the matched content; return `true`;
// else :
//     return `false`; (state of `buf` is unchanged)
static bool passover(BufType& buf, char const* match) {
	return buf.passover(match);
//	NOTE: more generic version if BufType needs to change.
// 	BufType::PosType pos = buf.tell();
// 	for (; *match; ++match) {
// 		if (buf.peek() != *match) {
// 			buf.seek(pos);
// 			return false;
// 		} else {
// 			buf.get();
// 		}
// 	}
// 	return true;
}

// string utils
static std::string trim(std::string const& s, char const* set = WHITESPACE) {
	std::string::size_type start = 0, end = s.size();

	for (; member(s[start], set); ++start) 
	{/* nop */}
	for (; member(s[end - 1], set); --end) 
	{/* nop */}

	return s.substr(start, end - start);
}

static std::string str_tolower(std::string const& s) {
	typedef std::string::size_type Sz;

	std::string res;
	res.reserve(s.size());

	for (Sz i = 0; i < s.size(); ++i)
		res.push_back( tolower(s[i]) );
	return res;
}

HttpParser::HttpParser(std::string const& newline) 
	: _nl(newline), _status(START), _method(), _uri(), _header(), _last_key() 
{}

// getters
HttpParser::Status 	HttpParser::status() const { return _status; }
std::string const& 	HttpParser::method() const { return _method; }
std::string const& 	HttpParser::uri()    const { return _uri; }

std::map<std::string, std::string> const& HttpParser::header() const {
	return _header;
}
	
HttpParser::Status HttpParser::parse_line(BufType& buf) {
	switch (_status) {
		case START:
			_parse_start_line(buf);
			break;
		case PREHEADER:
		case HEADER:
			_parse_header_line(buf);
			break;
		case PREBODY:
			return _status;
			break;
 		default:
 			break;
	}
	if ( !passover(buf, _nl.data()) )
		throw HttpError(400);
	return _status;
}

HttpParser::Status HttpParser::parse_some(BufType& buf) {
	int nlines = buf.count_lines();
	if (nlines == 0 && buf.full())
		throw HttpError(431);
	for (int i = 0; i < nlines; ++i)
		parse_line(buf);
	return _status;
}

void HttpParser::_parse_start_line(BufType& buf) {
	_method = untilset(buf, WHITESPACE);
	if ( !passover(buf, " ") )
		throw HttpError(400);
	std::string const SUPPORTED_METHODS[3] = { "GET", "POST", "DELETE" };
	if ( !member(_method, SUPPORTED_METHODS, 3) )
		throw HttpError(501);

	_uri = untilset(buf, WHITESPACE); 
	if ( !passover(buf, " ") )
		throw HttpError(400);

	if ( _uri.size() == 0 || _uri[0] != '/' )
		throw HttpError(400);

	if ( !passover(buf, "HTTP/1.1") )
		throw HttpError(505);

	_status = PREHEADER;
}

void HttpParser::_parse_header_line(BufType& buf) {
	using std::string;

	if ( member(buf.peek(), " \t") ) {
		if (_status == HEADER) {
			_header[_last_key] += untilset(buf, "\r");
			_header[_last_key] = trim( _header[_last_key] );
			return ;
		} else {
			throw HttpError(400);
		}
	}

	string key = str_tolower(untilset(buf, ":\r"));
	string value;
	switch (buf.peek()) {
		case '\r':
			if (key == "") {
				_status = PREBODY;
				return ;
			}
			else {
				throw HttpError(400);
			}
			break;
		case ':' :
			buf.get();
			value = trim( untilset(buf, "\r") );

			if ( _header.find(key) == _header.end() )
				_header[key] = value;
			else 
				_header[key] += ", " + value;
			_last_key = key;
			break;
		default:
			throw HttpError(400);
			break;
	}
	_status = HEADER;
}
