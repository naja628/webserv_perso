#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <cstddef>
#include "nat_utils.hpp"
#include "path_utils.hpp"

#include "Conf.hpp"

/* ParsingException */
ParsingException::ParsingException(std::istream & in, std::string const& errmsg)
	: _errmsg(errmsg), _iline(0)
{
	// save stream position where error occured
	in.clear();
	std::istream::pos_type errpos = in.tellg();

	// use getline to find line where error is
	in.seekg(0);
	while (in.tellg() <= errpos) {
		getline(in, _badline);
		++_iline;
	}

	// reset stream position
	in.seekg(errpos);
}

const char * ParsingException::what() const throw() {
	return _errmsg.data();
}

void ParsingException::print_error(std::ostream & out) const {
	out << "Parsing Exception: " << _errmsg << "\n";
	out << "On line " << _iline << ":\n";
	out << _iline << " | " << _badline << "\n";
}

/* PathConf */

PathConf const PathConf::_dfl_conf(0xff); // set default config with private ctor below

PathConf::PathConf(int mask, bool dirlist)
	: _setmask(mask), _directory_listing(dirlist), _super(NULL) 
{}

int PathConf::_name_to_mask(std::string const& opt_name) {
	const int N = 8;
	std::string const names[N] = {
		"accepted_methods", "redirect", "root", "directory_listing",
		"index", "cgi_execute", "uploads_directory", "path"
	};
	//
	int const masks[N] = { 
		METHODS, REDIR, ROOT, DIRLIST, 
		INDEX, CGIEXE, UPDIR, SUBPATH
	};
	//
	std::map<std::string, int> m;
	if ( m.count(names[0]) == 0) { // first call
		for (int i = 0; i < N; ++i)
			m[names[i]] = masks[i];
	}
	//
	return m.count(opt_name) ? m[opt_name] : 0;
}

PathConf::PathConf(PathConf * super) 
	: _setmask(0), _super(super)
{}

typedef std::set<std::string> SSet;

/* GETTERS */
SSet const& PathConf::accepted_methods() const {
	return _get(&PathConf::_accepted_methods, METHODS);
}

bool PathConf::is_redirected() const {
	if (_setmask & REDIR)
		return true;
	else if (!_super)
		return false;
	else
		return _super->is_redirected();
}

std::string PathConf::redirect_location(std::string const& path) const {
	if (!is_redirected())
		return ""; // maybe throw
	Redirect const& redir = _get(&PathConf::_redirect, REDIR);
	std::string loc = path;
	loc.replace(0, redir.from.size(), redir.to);
	return loc;
}

std::string const& PathConf::root() const {
	return _get(&PathConf::_root, ROOT);
}

bool PathConf::directory_listing() const {
	return _get(&PathConf::_directory_listing, DIRLIST);
}

std::string const& PathConf::index() const {
	return _get(&PathConf::_index, INDEX);
}

PathConf::ExeMap const& PathConf::cgi_execute() const {
	return _get(&PathConf::_cgi_execute, CGIEXE);
}

std::string const& PathConf::uploads_directory() const {
	return _get(&PathConf::_uploads_directory, UPDIR);
}


/* STREAM UTILS */

#define SP " \t\v\n\r"
static std::istream & skipset(std::istream & in, char const* chset) {
	// not chset -> UB
	while ( member(in.peek(), chset) )
		in.get();
	return in;
}

static bool expect(std::istream & in, char const* expected, const char * skip = SP) {
	// ! expected -> UB
	if (!in)
		return false;
	//
	skipset(in, skip);
	typedef std::istream::pos_type PosType;
	PosType save_pos = in.tellg();
	while (*expected) {
		if (in.get() != *expected) {
			in.seekg(save_pos);
			return false;
		}
		++expected;
	}
	return true;
}

static std::istream & read_quoted(
	   	std::istream & in, std::string & result, char const* skip = SP)
{
	if ( !expect(in, "\"", skip) ) {
		in.setstate(std::ios_base::failbit);
		return in;
	}
	getline(in, result, '\"');
	return in;
}


/* Paths */
Paths::Paths() : _path_map() {}

Paths::Paths(Paths const& other) : _path_map(other._path_map) {
	std::map<PathConf const*, PathConf *> pmap;
	typedef std::map<std::string, PathConf>::const_iterator It;
	for (It it = other._path_map.begin(); it != other._path_map.end(); ++it) {
		PathConf & elt = _path_map[it->first] = it->second;
		pmap[&(it->second)] = &elt;
		if (elt._super)
			elt._super = pmap[elt._super];
	}
}

Paths & Paths::operator=(Paths other) {
	using std::swap;
	swap(*this, other);
	return *this;
}

PathConf & Paths::_parse_one(std::istream & in, std::string super_path) {
	// we should have just read 'path' from outside
	std::string mapped_path;
	read_quoted(in, mapped_path);
	mapped_path = simplify_path(super_path + "/" + mapped_path);

	PathConf & conf = _path_map[mapped_path];

	if (!expect(in, "{"))
		throw ParsingException(in, "expected '{'");
	while ( !expect(in, "}") ) {

		std::string directive;
		if (! (in >> directive) )
			throw ParsingException(in, "unexpected eof");

		int directive_mask = PathConf::_name_to_mask(directive);
		conf._setmask |= (directive_mask > 0) ? directive_mask : 0;
		PathConf * sub; // silence "crosses init" warning
		switch (directive_mask) {
			case 0:
				throw ParsingException(in, "unknown directive " + directive);
				break;
			case PathConf::SUBPATH:
				sub = &_parse_one(in, mapped_path);
				sub->_super = &conf;
				break;

			case PathConf::METHODS:
				_read_set(in, conf._accepted_methods);
				break;
			case PathConf::REDIR:
				conf._redirect.from = mapped_path;
				conf._redirect.to = _read_single(in);
				break;
			case PathConf::ROOT:
				conf._root = _read_single(in);
				break;
			case PathConf::DIRLIST:
				conf._directory_listing = _read_yesno(in);
				break;
			case PathConf::INDEX:
				conf._index = _read_path(in, mapped_path);
				break;
			case PathConf::CGIEXE:
				_read_cgiexe(in, conf._cgi_execute);
				break;
			case PathConf::UPDIR:
				conf._uploads_directory = _read_path(in, mapped_path);
				break;
				// impossible path
			default:
				throw ParsingException(in, "unexpected error");
		}
	}
	return conf;
} ////////

void Paths::_read_set(std::istream & in, SSet & set) {
	std::string elem;
	while ( !expect(in, ";") ) {
		if ( !read_quoted(in, elem) )
			throw ParsingException(in, "bad syntax");
		set.insert(elem);
	}
}

std::string Paths::_read_single(std::istream & in) {
	std::string result;
	if ( !read_quoted(in, result) )
		throw ParsingException(in, "bad syntax");
	if ( !expect(in, ";") )
		throw ParsingException(in, "expected ';'");
	return result;
}

bool Paths::_read_yesno(std::istream & in) {
	std::string yesno = _read_single(in);
	if (yesno == "yes")
		return true;
	if (yesno == "no")
		return false;
	throw ParsingException(in, "expected 'yes' or 'no'");
}

std::string Paths::_read_path(std::istream & in, std::string const& prefix) {
	std::string result = _read_single(in);
	if (result.size() > 0 && result[0] == '/') // abs path
		return simplify_path(result);
	else // relative path
		return simplify_path(prefix + "/" + result);
}

void Paths::_read_cgiexe(std::istream & in, PathConf::ExeMap & map) {
	std::string runcmd;
	if (! read_quoted(in, runcmd) )
		throw ParsingException(in, "bad quoting");
	if (runcmd.size() == 0 || runcmd[0] != '/')
		throw ParsingException(in, "expected absolute (system-wise) path");
	while ( !expect(in, ";") ) {
		std::string ext;
		if (! read_quoted(in, ext) )
			throw ParsingException(in, "bad quoting");
		map[ext] = runcmd;
	}
}

PathConf const* Paths::get_conf (
		std::string const& path, std::string & translated_path) const
{
	std::string::size_type iquery = path.find('?');
	std::string query = 
		(iquery != std::string::npos ? path.substr(iquery) : "");

	std::string prefix = simplify_path(path.substr(0, iquery));
	std::string canon_path = prefix + query; // now normalized

	std::string::size_type islash = std::string::npos;
	do {
		prefix = prefix.substr(0, islash);
		if (prefix == "")
			prefix = "/";
		if ( _path_map.count(prefix) ) {
			PathConf const& conf = _path_map.at(prefix);
			translated_path = 
				conf.root() + (islash >= canon_path.size()? "" : canon_path.substr(islash));
			return &conf;
		}
	} while (prefix != "/" && ( islash = prefix.rfind('/') ) != std::string::npos);
	return NULL;
}

/* ServerConf */

ServerConf::ServerConf() : _max_body(0) {}

PathConf const* ServerConf::path_conf (
	std::string const& webpath, std::string & locpath) const
{
	return _paths.get_conf(webpath, locpath);
}

PathConf const* ServerConf::path_conf(std::string const& webpath) const {
	std::string discard;
	return path_conf(webpath, discard);
}

std::string ServerConf::translate_path(std::string const& webpath) const {
	std::string result;
	path_conf(webpath, result);
	return result;
}

std::string const* ServerConf::error_page(int errcode) const {
	size_t i_errpage;
	if (_code_to_index.count(errcode)) {
		i_errpage = _code_to_index.at(errcode);
		if ( i_errpage < _error_pages.size() )
			return &_error_pages[ i_errpage ];
	}
	return NULL;
}
	
long ServerConf::max_body() const {
	return _max_body;
}

void ServerConf::info(std::ostream & out) const {
	out << "Server [" 
		<< ( _names.empty() ? "anonymous" : *_names.begin() )
		<< "]:\n";
	if (_names.size() > 1) {
		out << "\talternative names: ";
		print_range(++_names.begin(), _names.end());
	}
	out << "\tports: ";
	print_range(_ports.begin(), _ports.end());
}

/* VirtualServers */

VirtualServers::VirtualServers() {}

VirtualServers::VirtualServers(std::istream & in) {
	parse_conf(in);
}

std::set<int> const& VirtualServers::ports() const {
	return _port_set;
}

MimeMap const& VirtualServers::mime_map() const {
	return _mime_map;
}

// VirtualServers const& VirtualServers::parse_conf(std::istream & in) {
void VirtualServers::parse_conf(std::istream & in) {
	std::string directive;
	while ( (in >> directive) ) {
		if (directive == "mime_types") {
			std::string mime_file = Paths::_read_single(in);
			parseMimeTypes(mime_file, _mime_map, in);
			continue;
		} else if (directive != "server") {
			throw ParsingException(in, "unknown toplevel directive " + directive);
		}

		// enter 'server' block
		if (!expect(in, "{"))
			throw ParsingException(in, "expected '{'");
		// put new `ServerConf` obj in servers and prepare to work on it
		_servers.push_back(ServerConf());
		ServerConf & conf = _servers.back();

		std::string subdirective;
		while ( !expect(in, "}") ) {

			std::string subdirective;
			if (! (in >> subdirective) )
				throw ParsingException(in, "unexpected eof / reading error");

			// read a directive
			switch (_name_to_code(subdirective)) {
				default: 
					throw ParsingException(in, "unknown server directive '" + subdirective + "'");
				case PORTS:
					_read_ports(in, conf);
					break;
				case NAMES:
					Paths::_read_set(in, conf._names);
					break;
				case BODY_SZ:
					in >> conf._max_body;
					if (!in)
						throw ParsingException(in, "invalid size");
					if (!expect(in, ";"))
						throw ParsingException(in, "expected ';'");
					break;
				case ERRPAGE:
					_read_errpage(in, conf);
					break;
				case PATH:
					conf._paths._parse_one(in, "/");
					break;
			}
		}
		if ( !conf.path_conf("/") )
			throw ParsingException(in, "missing `path \"/\" { root ...; ...}` block");
	}
}

void VirtualServers::_read_ports(std::istream & in, ServerConf & conf) {
	int port;
	while ( (in >> port) ) {
		port &= 0xffff; // restrain to valid port values (16 bit unsigned)
		conf._ports.insert(port);
		_port_set.insert(port);
	}
	in.clear();
	if ( !expect(in, ";") ) 
		throw ParsingException(in, "expected ';'");
}

void VirtualServers::_read_errpage(std::istream & in, ServerConf & conf) {
	std::string errpage;
	if ( !read_quoted(in, errpage) )
		throw ParsingException(in, "expected quoted path");
	errpage = simplify_path("/" + errpage);

	// append to error pages
	std::vector<std::string> & v = conf._error_pages;
	if ( v.end() == find(v.begin(), v.end(), errpage) )
		v.push_back(errpage);

	// map the wanted codes to the page we just added
	int errcode;
	while ( (in >> errcode) ) {
		conf._code_to_index[errcode] = conf._error_pages.size() - 1;
	}

	in.clear();
	if ( !expect(in, ";") )
		throw ParsingException(in, "expected ';'");
}

VirtualServers::DirCode VirtualServers::_name_to_code(std::string const& opt_name) {
	const int N = 5;
	static std::string const names[N] = {
		"ports", "server_names", "max_body", "error_page", "path"
	};

	static DirCode const codes[N] = { PORTS, NAMES, BODY_SZ, ERRPAGE, PATH };

	std::map<std::string, DirCode> m;
	if ( !m.count(names[0])) { // first call
		for (int i = 0; i < N; ++i)
			m[names[i]] = codes[i];
	}

	return m.count(opt_name) ? m[opt_name] : UNKNOWN;
}

ServerConf const* VirtualServers::get_server_conf(int port, std::string const& host) const
{
	ServerConf const* dfl = NULL;
	for (size_t i = 0; i < _servers.size(); ++i) {
		if (_servers[i]._ports.count(port)) {
			dfl = dfl? dfl : &_servers[i];
			if (_servers[i]._names.count(host))
				return &_servers[i];
		}
	}
	return dfl;
}

std::vector<ServerConf> const& VirtualServers::servers() const {
	return _servers;
}

/////////////////
/* parseMimeTypes */

void parseMimeTypes(std::string const& filePath, MimeMap& mimeTypes, std::istream & in)
{
	std::ifstream file(filePath.c_str());
	if (!file)
	{
		throw ParsingException(in, "couldn't open mime file");
	}

	std::string line;
	bool parsingBlock = false;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue ;

		std::istringstream iss(line);
		std::string key;
		std::string extension;

		if (!(iss >> key >> extension))
		{
			if (key == "}" && parsingBlock)
				return ;
			throw ParsingException(in, "mime: error parsing line: " + line);
		}

		if (key == "types" && extension == "{")
		{
			if (parsingBlock)
			{
				throw ParsingException(in, "mime: unexpected '{'");
			}
			parsingBlock = true;
			continue ;
		}

		if (extension.empty() || extension[extension.length() - 1] != ';')
		{
			throw ParsingException(in, "mime: expected ';'");
		}
		extension = extension.substr(0, extension.length() - 1);

		mimeTypes.insert(ConstPair(extension, key));
	}

	file.close();
}

#undef SP
