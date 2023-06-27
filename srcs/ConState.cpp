#include <fstream>

// C libs
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <poll.h>
#include <dirent.h>
#include <iomanip>
#include <sys/time.h>
#include <cstdio>

// Internal
#include "Writer.hpp"
#include "HttpParser.hpp"
#include "HttpError.hpp"
#include "nat_utils.hpp" 
#include "ConState.hpp"
#include "Buf.hpp"
#include "CgiHandler.hpp"

static std::string id_stamp(int fd) {
	return "id = " + strof(fd) + " | ";
}

// DEBUG
//#include <iostream>

//void print_header(HttpParser const& pa)
//{
//	std::cout << "++++++++++++++++++++++++++++++" << std::endl;
//	std::cout << pa.method() << " "  << pa.uri() << "\n";
//
//	typedef std::map<std::string, std::string>::const_iterator It;
//	for (It it = pa.header().begin(); it != pa.header().end(); ++it) 
//		std::cout << it->first << "_:_" << it->second << "\n";
//	std::cout << "++++++++++++++++++++++++++++++" << std::endl;
//}

ConState::ConState(int fd, VirtualServers const* confs, int port)
	: _fd(fd), _port(port), _event_set(POLLIN), _confs(confs),
      _cgi(), _call_next(&ConState::_get_first_request)
{}

void ConState::init(int fd, VirtualServers const* confs, int port) {
	_fd = fd;
	_port = port;
	_event_set = POLLIN;
	_confs = confs;
	_pconf = NULL;
	_call_next = &ConState::_get_first_request;
}

short ConState::_get_new_request() {
	_pa = HttpParser();
	_call_next = &ConState::_get_request;
	return _get_request();
}

short ConState::_get_first_request() {

	BufAdaptor wbuf(_in_buf);
	_pa.parse_some(wbuf);
	if (_pa.header().count("host"))
	{
		std::string host = _pa.header().at("host");
		if (host.find(':') != std::string::npos)
			host = host.substr(0, host.find(':'));
		_pconf = _confs->get_server_conf(_port, host);

		if (!_pconf)
			return 0;

		std::clog 
			<< "New connection on port [" << _port << "] |"
			<< " id = " << _fd << "\n";

		_cgi.setFilename();
		_cgi.setConf(_pconf);
		if ( _pa.status() == HttpParser::PREBODY )
			return _dispatch();
		else 
			return _get_request();

	} else {
		if (_pa.status() == HttpParser::PREBODY) // -> Error missing 'host' field
			throw HttpError(400);
		_call_next = &ConState::_get_first_request;
		return POLLIN;
	}
}

short ConState::_get_request()
{
	_call_next = &ConState::_get_request;
	BufAdaptor wbuf(_in_buf);
	if ( _pa.parse_some(wbuf) != HttpParser::PREBODY )
		return POLLIN; // still parsing
	else
		return _dispatch();
}

short ConState::_dispatch() {

	std::clog << id_stamp(_fd) << "New request: " << _pa.method() << " " << _pa.uri() << " HTTP/1.1\n";
	PathConf const* path_conf = _pconf->path_conf(_pa.uri());
	if ( !path_conf )
		throw HttpError(404);
	if ( !path_conf->accepted_methods().count(_pa.method()) ) // method not accepted
		throw HttpError(405);
	if ( path_conf->is_redirected() ) {
		// prep redir
		_wr.reset();
	   	_wr.set_status(308); // 301?
		_wr.add_header_field("location", path_conf->redirect_location(_pa.uri()));
		_call_next = &ConState::_write;
		return POLLOUT;
	}

	if ( _pa.method() == "POST" )
	{
		_chunk_streamer = ChunkStreamer(); // reset

		_cgi.setMethod("POST");

			return _read_body();
	}

	if ( _pa.method() == "GET" )
	{
		_cgi.setMethod("GET");
		return _prepare_page();
	}

	if ( _pa.method() == "DELETE" )
	{
		bool failed = std::remove( (_pconf->translate_path(_pa.uri())).data() );
		if (failed)
			throw HttpError(404);

		_wr.reset();
		_wr.set_status(204);
		_call_next = &ConState::_write;
		return POLLOUT;
	}

	return 0; // unknown method, should have caught exn before
}

short ConState::_read_body()
{
	_cgi.openFile(0);
	try {
		_call_next = &ConState::_read_body;
		if (   _pa.header().count("transfer-encoding")
				&& _pa.header().at("transfer-encoding") == "chunked" )
		{
			if ( _pconf->max_body() > 0 )
				_chunk_streamer.set_max_size(_pconf->max_body() );
			if ( _chunk_streamer.read_some_chunked(_in_buf, _cgi) != ChunkStreamer::DONE)
				return POLLIN; // still streaming the body

		}
		else if ( _pa.header().count("content-length") )
		{
			if (_chunk_streamer.status() == ChunkStreamer::START)
			{
				ssize_t length = from_str<int>(_pa.header().at("content-length"), HttpError(400));
				if (length < 0)
					throw HttpError(400);
				if ( (_pconf->max_body() > 0) && length > _pconf->max_body() )
					throw HttpError(413);
				_chunk_streamer.set_chunk_size(length);
			}

			if ( _chunk_streamer.read_some_single(_in_buf, _cgi) != ChunkStreamer::DONE)
				return POLLIN; // still streaming the body
							   // body done
		}
	} catch (HttpError & e) {
		_cgi.closeFile();
		throw ;
	}
	_cgi.closeFile();
		
	return _prepare_page();
}

std::string	ConState::_dir_list(DIR *dir, std::string path)
{
	std::ostringstream	ss_buf;
	ss_buf << "<html>\n<head><title>Index of " << _pa.uri() << "</title></head>\n";
	std::string prevdir_link = 
		_pa.uri() + (_pa.uri()[_pa.uri().size() - 1] == '/' ? "" : "/") + "../";
	ss_buf 
		<< "<body>\n<h1>Index of " << _pa.uri() 
		<< "</h1><hr><pre>"
	    << "<a href=\"" << prevdir_link << "\">../</a>\n";

	struct dirent	*ent;
	while ((ent = readdir(dir)) != NULL)
	{
		std::string	name = ent->d_name;
		if (name == "." || name == ".." || name[0] == '.')
			continue ;

		std::string link;
		if (*(_pa.uri().rbegin()) == '/')
			link = _pa.uri() + name;
		else 
			link = _pa.uri() + "/" + name;
		ss_buf << "<a href=\"" << link << "\">";
		if (name.size() > 50)
		{
			name.resize(47);
			name += "..&gt;";
		}
		ss_buf << name << "</a>";
		if (name.size() > 51)
			ss_buf << " ";

		if (access((path + "/" + name).data(), R_OK) == -1)
			continue ;	// skip bc no access
		ss_buf << "\n";
	}
	ss_buf << "</pre><hr></body>\n</html>";
	std::string	str_buf = ss_buf.str();

	closedir(dir);
	return (str_buf);
}

void ConState::_choose_write_method() {
	if (rem_stream_size(_resp_file.get()) < STREAM_THRESHOLD) {
		_wr.read_body_from_stream(_resp_file.get());
		_resp_file.get().close();
	} else {
		_wr.stream_file_to_body(_resp_file.get());
	}
}

short ConState::_prepare_page() {

	_wr.reset();
	// find relevant path_conf :
	std::string locpath;
	PathConf const* path_conf = _pconf->path_conf(_pa.uri(), locpath);
	if (!path_conf) {
		throw HttpError(404);
	}

	_cgi.setParser(_pa);	// dans isCgi()
	_cgi.setRoot(locpath);	// dans isCgi()
	if (_cgi.isCgi(path_conf->cgi_execute()) == true)
	{
		_cgi.run();
		_call_next = &ConState::_wait_cgi;
		return (_wait_cgi());
	}

	// response common part:
	_wr.set_status(200);

	DIR * dir;
	if ( (dir = opendir(locpath.data())) ) { // if directory
		// try to use 'index'
		std::string index_path = path_conf->index();
		if (index_path != "")
			_resp_file.get().open(_pconf->translate_path(index_path).data());
		if (_resp_file.get().is_open())
	   	{
			closedir(dir); // won't be needed

			_wr.add_header_field(
					"content-type",
				   	get_mime_type(index_path, _confs->mime_map() ));
			_choose_write_method();
		// try directory listing
		} else if ( path_conf->directory_listing()) {
			_wr.add_header_field("content-type", "text/html");
			std::string	tmp_str = _dir_list(dir, locpath);
			_wr.use_as_body(tmp_str); // note : it's fine that `tmp_str` goes 
									  // out of scope
		} else {
			closedir(dir);
			throw HttpError(404);
		}
	} else { // serve normal page
		_resp_file.get().open(locpath.data());
		if (!_resp_file.get().is_open()) {
			throw HttpError(404);
		}
		_wr.add_header_field("content-type", get_mime_type(locpath, _confs->mime_map() ) );
		_choose_write_method();
	}
	_call_next = &ConState::_write;
	return POLLOUT;
}

short ConState::_wait_cgi() {
	if (_cgi.waitChild() == true)
		return POLLOUT;
	else
		return _prepare_cgi_page();
}

static std::string trim(std::string const& s) {
	std::string::size_type i, j;
	i = s.find_first_not_of("\t ");
	j = s.find_last_not_of("\t ");
	return (i == std::string::npos ? s : s.substr(i, j));
}
	
static std::string str_tolower(std::string const& s) {
	typedef std::string::size_type Sz;

	std::string res;
	res.reserve(s.size());

	for (Sz i = 0; i < s.size(); ++i)
		res.push_back( tolower(s[i]) );
	return res;
}

short ConState::_prepare_cgi_page()
{
	_resp_file.get().open(_cgi.outFileName().data());
	std::ifstream & ifs(_resp_file.get());
	if (!ifs.is_open())
		throw HttpError(500);

	std::map<std::string, std::string> response_header;
	while (true) {
		std::string line;
		std::getline(ifs, line, '\n');
		if (ifs.fail())
			throw HttpError(500);
		if (line == "")
			break;
		std::string::size_type icolon = line.find(':');
		if (icolon == std::string::npos)
			throw HttpError(500);
		response_header[str_tolower(line.substr(0, icolon))] 
			= trim(line.substr(icolon + 1));
	}

	_wr.reset();
	if (response_header.count("status")) {
		_wr.set_status(from_str<int>(response_header.at("status"), HttpError(500)));
		response_header.erase("status");
	} else {
		_wr.set_status(200);
	}

	typedef std::map<std::string, std::string>::iterator It;
	for (It it = response_header.begin(); it != response_header.end(); ++it)
		_wr.add_header_field(it->first, it->second);

	if (!response_header.count("content-type"))
		_wr.add_header_field("content-type", "text/html");

	_choose_write_method();
	_call_next = &ConState::_write;
	return POLLOUT;
}

short ConState::_write() {
	switch (_wr.write_some(_fd)) {
		default:
		case Writer::WRITE_ERROR:
			return 0; // close connection
		case Writer::READ_ERROR:
			throw HttpError(500);
		case Writer::PARTIAL_WRITE:
			return POLLOUT;
		case Writer::OK_FINISHED:
			_resp_file.get().close();
			_call_next = &ConState::_get_new_request;
			std::clog << id_stamp(_fd) << "Response sent. Ready for new request\n";
			return POLLIN;
	}
}

short ConState::_write_then_close() {
	switch (_wr.write_some(_fd)) {
		default:
		case Writer::WRITE_ERROR:
			return 0; // close connection
		case Writer::PARTIAL_WRITE:
			return POLLOUT;
		case Writer::OK_FINISHED:
			_resp_file.get().close();
			std::clog << id_stamp(_fd) << "Response sent. Closing connection\n";
			return 0;
	}
}

// helper to _prepare_error
static void add_allowed_methods(Writer & wr, PathConf const* conf) {
	std::ostringstream ss; // pass to `print_range` to get str

	std::set<std::string> const& methods = conf->accepted_methods();
	print_range(methods.begin(), methods.end(), ", ", "", ss);

	wr.add_header_field("allow", ss.str());
}

std::string ConState::_generate_error_page(int errcode) const
{
	std::stringstream ss;
	ss << "<html><body><center><h1>" << strof(errcode);
	ss << "</h1></center><hr><center>webserv default error page</center>";
	ss << "</body></html>";
	std::string ret = ss.str();
	return ret;
}

void ConState::_prepare_error(HttpError e) {
	_wr.reset();
	_wr.set_status(e);
	_wr.add_header_field("content-type", "text/html");

	if (e == 405) // 'allow' field needed to specify supported methods
		add_allowed_methods(_wr, _pconf->path_conf(_pa.uri()));

	std::string const* error_path;
	if ( (error_path = _pconf->error_page(e)) ) {
		std::string translated_path = _pconf->translate_path(*error_path);
		_resp_file.get().open(translated_path.data());
		if (!_resp_file.get().is_open())
			_wr.body_append(_generate_error_page(e));
		_choose_write_method();
	} else {
		_wr.body_append(_generate_error_page(e));
	}
}

short ConState::operator() (short pollflags)
{
	int recvd;
	if (_event_set & POLLIN && pollflags & POLLIN) { // normal read
		recvd = _in_buf.read_more(_fd);
	} else if (pollflags & POLLIN) { // check if client has closed 
		char dummy;
		recvd = recv(_fd, &dummy, 1, MSG_PEEK);
	}

	if (recvd <= 0)
	{
		return (_event_set = 0); // will close connection
	}

// 	if (pollflags & POLLIN) 
// 	{
// 		// TODO maybe problem reading in advance, when polling for POLLIN
// 		// to be able to detect client closing connections
// 		// when the buffer becomes full
// 		// cf bookmarked stack-overflow for maybe solution
// 		recvd = _in_buf.read_more(_fd);
// 
// 	}

	if (pollflags & _event_set) { // event we're looking for
		try {
			_event_set = (this->*_call_next)();
		} catch (HttpError e) {
			std::clog << id_stamp(_fd) 
				<< "Handling Http error [" << e << "]\n";
			_prepare_error(e);
			if (_chunk_streamer.status() == ChunkStreamer::DONE)
				_call_next = &ConState::_write;
			else 
				_call_next = &ConState::_write_then_close;
			return (_event_set = POLLOUT);
		}
	}
	return _event_set;
}

short ConState::event_set() const
{
	return _event_set;
}
