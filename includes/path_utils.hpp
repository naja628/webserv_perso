#ifndef PATH_UTILS_HPP
# define PATH_UTILS_HPP

# include <string>

class BadCwd { // needed bc constraints of subject don't allow for `getcwd`
private:
	static std::string _cwd;

	friend std::string bad_getcwd();
	friend bool bad_setcwd(char ** env);
};

// look for PWD env variable in env
// and assume it is the current working dir for later calls 
// to `bad_getcwd`.
// Return : 
// 	* found -> true
// 	* not found -> false (cwd is set to ".")
bool bad_setcwd(char ** env);
std::string bad_getcwd();

// translate '.' and '..', dedup slashes and, rm trailing slashes
std::string simplify_path(std::string const& path);

// return `simplify_path(bad_getcwd() + "/" + path)`
std::string canonize_path(std::string const& path);

#endif
