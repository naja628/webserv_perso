#ifndef CONSTATE_HPP
# define CONSTATE_HPP

# include "HttpParser.hpp"
# include "HttpError.hpp"
# include "Buf.hpp"
# include "Writer.hpp"
# include "ChunkStreamer.hpp"
# include "Conf.hpp"
# include <sys/types.h>
# include <dirent.h>

# include "HttpParser.hpp"
# include "Buf.hpp"
# include "Writer.hpp"
# include "ChunkStreamer.hpp"
# include "Conf.hpp"
# include "CgiHandler.hpp"

class ConState {
public:
	// TODO accepting NULL is a bad idea
	ConState(int fd = -1, VirtualServers const* confs = NULL, int port = -1);
	void init(int fd, VirtualServers const* confs, int port);
	//void init_cgi();
	short operator()(short pollflags);
	short event_set() const;

private:
	static const size_t STREAM_THRESHOLD = 8192; // above this size don't try to
												 // allocate the whole response body
												 // at once
	typedef short (ConState::*CallBack)();

	int _fd;
	int _port;
	short _event_set;

	VirtualServers const* _confs;
	ServerConf const* _pconf;

	HttpParser _pa;
	Buf _in_buf;
	ChunkStreamer _chunk_streamer;
	Writer _wr;
	CgiHandler	_cgi;
	class WFile { // fake canonical wrapper
		std::ifstream _if;
		
	public:
		WFile() {}
		WFile(WFile const& other) : _if() {(void) other;}
		WFile & operator=(WFile const& other) { (void) other; return *this; }
		std::ifstream & get() { return _if; }
	};
	WFile _resp_file;



	CallBack _call_next;

	// "callbacks"
	short _get_new_request();
	short _get_first_request();
	short _dispatch();
	short _get_request();
// 	short _send_page();
	short _read_body();
	short _write();
	short _write_then_close();

	// in-betweens
	short _prepare_page();
	short _wait_cgi();
	short _prepare_cgi_page();
	void _prepare_error(HttpError e);
	
	// utils
	void ConState::_choose_write_method() {
	std::string	_dir_list(DIR *dir, std::string path);
	std::string _generate_error_page(int errcode) const;
	void _choose_write_method();
};

#endif
