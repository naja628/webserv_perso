#ifndef NAT_UTILS_HPP
# define NAT_UTILS_HPP

# include <iostream>

template <typename T, typename It>
bool member(T const& elt, It set) {
	T got;
	while ( (got = *(set++)) ) {
		if (got == elt)
			return true;
	}
	return false;
}

template <typename T, typename It>
bool member(T const& elt, It set, size_t sz) {
	for (size_t i = 0; i < sz; ++i) {
		if (elt == *(set++))
			return true;
	}
	return false;
}

template <typename It>
void print_range(
		It first, It last,
	   	char const* sep = ", ",
		char const* end = "\n",
		std::ostream & out = std::cout)
{
	if (first == last) {
		out << end;
		return ;
	}

	out << *(first++);
	for (It it = first; it != last; ++it)
		out << sep << *it;
	out << end;
}

#include <string>
#include <sstream>
template <typename T>
std::string strof(T const& t) {
	std::ostringstream ss;
	ss << t;
	return ss.str();
}

template <typename T, typename E>
T from_str(std::string const& s, E const& throw_me) {
	std::istringstream ss(s);
	T ret;
	ss >> ret;
	if (!ss)
		throw throw_me;
	return ret;
}

#define HZ_SPACE " \t" 
#define WHITESPACE " \t\r\v\n"

#endif
