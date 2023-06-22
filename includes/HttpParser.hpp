#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include <string>
# include <map>
# include "Buf.hpp"
// # include "RotBuf.hpp"

class BufAdaptor {
	// Wrapper around `Buf` object
	// provides convenient interface for HttpParser
	// (modifies its underlying `Buf` in place)

private:
	Buf & _buf;

	// Not copyable
	BufAdaptor(BufAdaptor const& other);
	BufAdaptor& operator=(BufAdaptor const& other);

public:
	static char const NOCHAR = -1;

	BufAdaptor(Buf & buf);

	// Extracts next char and return it.
	// returns BufAdaptor::NOCHAR if buf is empty
	char get();
	// get next char without extracting
	char peek() const;

	// number of full lines to be read (ie after `buf._pos`) in buf
	int count_lines(char const* nl = "\r\n") const;

	bool full() const;
	bool empty() const;

	// if buffer starts with `to_match` move read pos after just after
	// and return `true`.
	// otherwise do not move and return `false`
	bool passover(char const* to_match);
};

typedef BufAdaptor BufType; 
/* for the following code to work, BufType is required to meet the following
 * interface:
 *     * _.get() -> read next character
 *     * _.peek() -> check what would be returned by `get` without modification
 *     * 1. pos = _.tell(); 2. ...; 3. _.seek(pos); ->
 *          if no methods other than the above have been called in (2)
 *          (3) restore `_` (ie the buffer) to the state at (time of) (1)
 *          (note : (2) can contain any operation of the `HttpParser` class)
 *          (note : this is a LIE, use/uncomment more generic version of static 
 *          util `passover` in .cpp to make true again)
 *
 *     (needed only for `continue_parsing()`):
 *     * _.count_lines() -> returns the number of full lines in the buffer.
 *                          lines are assumed to be terminated by "\r\n".
 *     * _.full() -> returns true if the buffer is full
**/

class HttpParser { // Class used to parse an http request
				   // doesn't handle the parsing of the `body`

	// TODO maybe forbid non printable characters, etc
	// TODO maybe handle leading newlines before a request

public:
	HttpParser(std::string const& newline = "\r\n");
	// doesn't manage any raw resource so will behave canonically 

	/* GETTERS */
	enum Status { START, PREHEADER, HEADER, PREBODY };
	Status status() const; // what we will parse next

	// returns "" if `status() == START` (not already parsed)
	std::string const& 	method() const;
	std::string const& 	uri() const;

	// may return an incomplete header if `status() < PREBODY`
	// but some field might already be present if needed to take 
	// early decisions (if `status() >= HEADER`) 
	std::map<std::string, std::string> const& header() const;

// 	// 1 : returns true if there is an header field corresponding to `key`
// 	// 2 : ... and its value is `value`
// 	bool header_has(std::string const& key) const;
// 	bool header_has(std::string const& key, std::string const& value) const;

	/* PARSING */
	// `buf` SHALL contain at least 1 full line (terminated by "\r\n")
	// (UB otherwise)
	//
	// Consume one line from `buf` and parse the data it contains.
	// Returns the status after the operation. 
	// May throw an `HttpError`.
	Status parse_line(BufType & buf);

	// parses as many lines as the buffer contains calling `parse_line`
	// several times.
	Status parse_some(BufType & buf);

private:
	std::string 	_nl;
	Status			_status;
	std::string		_method;
	std::string		_uri;
	std::map<std::string, std::string> _header;

	std::string		_last_key;

	void _parse_start_line(BufType& buf);
	void _parse_header_line(BufType& buf);

};

#endif
