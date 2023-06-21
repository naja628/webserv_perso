#ifndef HTTPERROR_HPP
# define HTTPERROR_HPP

class HttpError {
private:
	int _errcode;

public:
	HttpError(int code);
	char const* reason() const;
	operator int() const;
};

char const* http_reason(int code);

#endif
