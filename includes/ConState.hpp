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
	ConState(int fd = -1, VirtualServers const* confs = NULL, int port = 4242);
	void init_cgi();
	short operator()(short pollflags);
	short event_set() const;

private:
	typedef short (ConState::*CallBack)();

	int _fd;
	int _port;
	short _event_set;

	VirtualServers const* _confs; // TODO this is ugly
	ServerConf const* _pconf;
	// maybe :
	// output_buffer
	// fds?

	HttpParser _pa; // probably change this
	Buf _in_buf;
	ChunkStreamer _chunk_streamer;
	Writer _wr;
	CgiHandler	_cgi;

	CallBack _call_next;

	short _get_new_request();
	short _get_first_request();
	short _dispatch();
	short _get_request();
// 	short _send_page();
	short _read_body();
	short _write();
	short _write_then_close();
	
	std::string	_dir_list(DIR *dir, std::string path);
	std::string _generate_error_page(int errcode) const;
	short _prepare_page();
	void _prepare_error(HttpError e);
};

#endif
