#ifndef CONF_HPP
# define CONF_HPP

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <cstddef>
#include <vector>
#include <exception>

// TODO maybe comments
// maybe add error for multiple directive with same name
// (except `error_page`)

class ParsingException: std::exception {
private:
	std::string _errmsg;
	std::string _badline;
	int _iline;

public:
	ParsingException(std::istream & in, std::string const& errmsg);
	~ParsingException() throw() {};
	const char * what() const throw();
	void print_error(std::ostream & out) const;
};

class Paths;
class VirtualServers;

typedef std::set<std::string> SSet;

//////////////////////////////
class PathConf {
// TODO maybe add support for relative paths
// problem: not always clear what they should mean
//
// TODO add match_path field or something
public:
	/* CTOR */
	PathConf(PathConf * super = NULL);

	/* GETTERS */
	SSet const& accepted_methods() const;

	bool is_redirected() const;
	std::string redirect_location(std::string const& path) const;

	std::string const& root() const; // nginx alias not nginx root
	bool directory_listing() const;
	std::string const& index() const;
	SSet const& cgi_extensions() const;
	std::string const& uploads_directory() const;

	// utils
// 	bool is_cgi(std::string path) const; // TODO either rm or handle
// 	                                     // cgi in the middle of path

	friend class Paths;

private:
	static const int 
		METHODS = 1, REDIR = 2, ROOT = 4, DIRLIST = 8, 
		INDEX = 16, CGIEXT = 32, UPDIR = 64, SUBPATH = -1;

	// default conf global object
	static PathConf const _dfl_conf;
	// special ctor for the default conf
	PathConf(int mask, bool dirlist = true);

	struct Redirect {
		std::string from;
		std::string to;
	};

	// "accepted_methods" -> METHODS
	// ...
	static int _name_to_mask(std::string const& opt_name);


	int _setmask; // _setmask & METHODS -> true when _accepted_methods is set
				  // _setmask & ... -> ...

	SSet _accepted_methods;
	Redirect _redirect;
	std::string _root;
	bool _directory_listing;
	std::string _index;
	SSet _cgi_extensions;
	std::string _uploads_directory;

	PathConf * _super; // config to default to for unset 'options'
					   // use (class global) _dfl_conf when NULL

	template <typename T> // generic getter
	T const& _get(T PathConf::*memvar, int mask) const {
		if (_setmask & mask)
			return this->*memvar;
		else 
			return (_super ? _super->_get(memvar, mask) : _dfl_conf.*memvar); 
	}
};

/////////////////////////////////////
class Paths {
public:

	// default ctor ok
	
	// find the configuration for the `path` (normally uri from http request)
	// returns a pointer the corresponding `PathConf` object
	//         or NULL if no match is found
	//
	// Sets `translated_path` to the "physical" path (local filesystem).
	PathConf const* get_conf(std::string const& path, std::string & translated_path) const;

private:

	std::map<std::string, PathConf> _path_map; // map of uris to conf objects
											   // (match longest prefix)
	
	PathConf & _parse_one(std::istream & in);

	// reading utils
	static SSet _read_set(std::istream & in);
	static std::string _read_single(std::istream & in);
	static bool _read_yesno(std::istream & in);
	static std::string _read_path(std::istream & in, std::string const& prefix);

	friend class VirtualServers;
};

//////////////////////////////////
class ServerConf {
public:
	ServerConf();

	// find the configuration for the `webpath` (normally uri from http request)
	// returns a pointer the corresponding `PathConf` object
	//         or NULL if no match is found
	//
	// Sets `locpath` to the translated "physical" path (local filesystem).
	PathConf const* path_conf
		(std::string const& webpath, std::string & locpath) const;
	// same as before but discards transalted path
	PathConf const* path_conf(std::string const& webpath) const;
	
	std::string translate_path(std::string const& webpath) const;

	// get error page (as pointer to string representing path) 
	// associated with http error `errcode`
	// NULL if not found
	std::string const* error_page(int errcode) const;

	// maximum size allowed for http entity bodies (for this server)
	// -1 if unset/unlimited
	long max_body() const;

	void info(std::ostream & out) const;

private:
	std::set<int> _ports;
	SSet _names;
	size_t  _max_body;
	std::vector<std::string> _error_pages;
	std::map<int, int> _code_to_index; // eg 404 -> 1 with 
									   //    _error_pages[1] = "not_found.html"
	Paths _paths;

	friend class VirtualServers;
};

////////////////////////////
typedef std::map<const std::string, const std::string> MimeMap;
typedef std::pair<const std::string, const std::string> ConstPair;

class VirtualServers {
public:
	VirtualServers();
	VirtualServers(std::istream & in);

// 	VirtualServers const& parse_conf(std::istream & in);
	std::set<int> const& ports() const;
	void parse_conf(std::istream & in);
	// note : can be invalidated by modifying `*this`
	ServerConf const* get_server_conf(int port, std::string const& hostname) const;
	MimeMap const& mime_map() const;
	std::vector<ServerConf> const& servers() const;


private:
	enum DirCode { UNKNOWN = 0, PORTS, NAMES, BODY_SZ, ERRPAGE, PATH };

	std::vector<ServerConf> _servers;
	std::set<int> _port_set;
	MimeMap _mime_map;

	static DirCode _name_to_code(std::string const& name);

	void _read_ports(std::istream & in, ServerConf & conf);
	static void _read_errpage(std::istream & in, ServerConf & conf);
};

void parseMimeTypes(std::string const& filePath, MimeMap& mimeTypes, std::istream & in);

#endif
