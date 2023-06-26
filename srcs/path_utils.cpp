#include <vector>
#include <string>
#include <cstring>
#include "path_utils.hpp"

std::string BadCwd::_cwd(".");

bool bad_setcwd(char ** env) {
	BadCwd::_cwd = ".";
	for (char * envvar = *env; envvar != NULL; envvar = *(++env)) {
		char * eqpos = strchr(envvar, '=');
		if (!eqpos)
			return false;
		if ( strncmp("PWD", envvar, 3) == 0 ) {
			BadCwd::_cwd.clear();
			BadCwd::_cwd.append(eqpos + 1, strlen(eqpos + 1));
			return true;
		}
	}
	return false;
}

std::string bad_getcwd() {
	return BadCwd::_cwd;
}

std::string simplify_path(std::string const& path) {
	bool is_abs_path = (path.size() >= 1 && path[0] == '/');
	std::vector<std::string> segments;

	std::string::size_type seg_start = 0, seg_stop;
	while (true) { // break inside
		// skip slashes
		while (seg_start < path.size() && path[seg_start] == '/')
			++seg_start;
		if (seg_start >= path.size()) // `>=` important bc could be `npos`
			break;

		// build current segment
		seg_stop = path.find('/', seg_start);
		std::string segment = path.substr(seg_start, seg_stop - seg_start);
		seg_start = seg_stop;

		// cases (update `segments`)
		if (segment == ".")
			continue;
		else if (segment == "..") {
			if (!segments.empty())
				segments.pop_back();
		} else {
			segments.push_back(segment);
		}
	}

	std::string res = is_abs_path ? "/" : "";

	if ( segments.empty() ) {
		return res;
	} else {
		res += segments.front();
		typedef std::vector<std::string>::const_iterator It;
		for (It it = ++segments.begin(); it != segments.end(); ++it) {
			res += "/" + *it;
		}
		return res;
	}
}

std::string canonize_path(std::string const& path) {
	return simplify_path(bad_getcwd() + "/" + path);
}
