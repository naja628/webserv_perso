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
		MATCH(400, "BAD REQUEST");
		MATCH(404, "NOT FOUND");
		MATCH(501, "NOT IMPLEMENTED");
		MATCH(505, "HTTP VERSION NOT SUPPORTED");
		MATCH(405, "METHOD NOT ALLOWED");
		// ...
		default: return "";
	}
}
#undef MATCH
