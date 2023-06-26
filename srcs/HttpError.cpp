#include "HttpError.hpp"
#include <cstddef>

HttpError::HttpError(int code) : _errcode(code) {}

HttpError::operator int() const {
	return _errcode;
}

#define MATCH(CODE, MSG) case CODE : return MSG; break
char const* HttpError::reason() const {
	switch (_errcode) {
		MATCH(200, "OK");
		MATCH(201, "OK CREATED");
		MATCH(204, "NO CONTENT");
		MATCH(308, "PERMANENT REDIRECT");
		MATCH(400, "BAD REQUEST");
		MATCH(404, "NOT FOUND");
		MATCH(405, "METHOD NOT ALLOWED");
		MATCH(413, "PAYLOAD TOO LARGE");
		MATCH(431, "REQUEST HEADER FIELDS TOO LARGE");
		MATCH(501, "NOT IMPLEMENTED");
		MATCH(505, "HTTP VERSION NOT SUPPORTED");
		// ...
		default: return "";
	}
}
#undef MATCH
