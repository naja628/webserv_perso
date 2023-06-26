#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "CgiHandler.hpp"
#include "Buf.hpp"
#include "Conf.hpp"
#include "HttpParser.hpp"
#include "HttpError.hpp"
#include "path_utils.hpp"


/****************************************************/
/*                                                  */
/*                  CONSTRUCTORS                    */
/*                                                  */
/****************************************************/
// default
CgiHandler::CgiHandler() : _pid(-1), _stopwatch(0)
{}

// copy
CgiHandler::CgiHandler(const CgiHandler &src)
	: _pid(-1), _stopwatch(0)
{
	*this = src;
}


/****************************************************/
/*                                                  */
/*                  DESTRUCTOR                      */
/*                                                  */
/****************************************************/
CgiHandler::~CgiHandler()
{
	if (_pid > 0)
		kill(_pid, SIGTERM);
	rmFile();
}


/****************************************************/
/*                                                  */
/*                  ASSIGN OPERATOR                 */
/*                                                  */
/****************************************************/
CgiHandler	&CgiHandler::operator=(const CgiHandler &src)
{
	_pa = src._pa;
	_header = src._header;
	_method = src._method;
	_fileBuf = src._fileBuf;
	_fileExec = src._fileExec;
	_root = src._root;
	return (*this);
}


/****************************************************/
/*                                                  */
/*                  SETTERS                         */
/*                                                  */
/****************************************************/
void	CgiHandler::setParser(HttpParser pa)
{
	_pa = pa;
	_header = pa.header();
}

void	CgiHandler::setMethod(std::string method)
{
	_method = method;
}

void	CgiHandler::setRoot(std::string root)
{
	_root = root;
}

void	CgiHandler::setConf(ServerConf const* conf) {
	_conf = conf;
}

/****************************************************/
/*                                                  */
/*                  FILES OPERATIONS                */
/*                                                  */
/****************************************************/
void	CgiHandler::openFile(int method)
{
	if (method == 0)
	{
		_fs_file.open(_fileBuf.data(), std::fstream::out | std::fstream::trunc);
		if (!_fs_file.is_open())
			throw HttpError(500);
	}
	else
	{
		_fs_file.open(_fileBuf.data(), std::fstream::in);
		if (!_fs_file.is_open())
			throw HttpError(500);
	}
}

void	CgiHandler::closeFile()
{
	_fs_file.close();
}

void	CgiHandler::rmFile()
{
	remove(_fileBuf.c_str());
	remove(_fileExec.c_str());
}

ssize_t	CgiHandler::bodyInFile(Buf & buf, size_t rem_chunk_size)
{
	size_t	data_size = buf.end - buf.pos;

	std::streampos	startPos = _fs_file.tellg();
	_fs_file.write(buf.pos, std::min(rem_chunk_size, data_size));
	std::streampos	endPos = _fs_file.tellg();

	return (endPos - startPos);
}

std::string	CgiHandler::_fileSize()
{
	if (_fileExists(_fileBuf) == false)
		return "";

	openFile(1);

	_fs_file.seekg(0, std::ios_base::end);
	size_t	size = _fs_file.tellg();
	_fs_file.seekg(0);

	closeFile();

	std::ostringstream ss;
	ss << size;
	return (ss.str());
}

std::string	CgiHandler::fileToStr()
{
	std::ifstream		ifs(_fileExec.data());
	std::stringstream	buf;

	buf << ifs.rdbuf();
	return (buf.str());
}

std::string const& CgiHandler::outFileName() const {
	return _fileExec;
}



/****************************************************/
/*                                                  */
/*                  LAUNCH                          */
/*                                                  */
/****************************************************/

bool	CgiHandler::waitChild()
{
	int	wstatus;
	if (!waitpid(_pid, &wstatus, WNOHANG))
	{
		if ((((float)(clock() - _stopwatch)) / CLOCKS_PER_SEC) >= CGI_TIMEOUT)
		{
			kill(_pid, SIGTERM);
			_pid = -1;
			throw HttpError(500);
		}
		return true;
	}
	if (WIFEXITED(wstatus) == false)
	{
		_pid = -1;
		throw HttpError(500);
	}
	_pid = -1;
	return false;
}

void	CgiHandler::run()
{
	if (_pid > 0)
		kill(_pid, SIGTERM);
	_stopwatch = clock();
	_pid = fork();
	if (_pid < 0)
		throw HttpError(500);
	if (!_pid)
		_launch();
}

bool	CgiHandler::isCgi(std::set<std::string> ext)
{
	std::string	path = _getPath();
	if (path == "")
		return false;

	return (_checkExtension(path, ext));
}


/****************************************************/
/*                                                  */
/*                  LAUNCH UTILS                    */
/*                                                  */
/****************************************************/
void	CgiHandler::_launch()
{
	int	fd1, fd2;

	std::string	path = _getPath();
	if (access(path.c_str(), X_OK) == -1)
		_exit(-1, -1);

	if ((fd1 = open(_fileBuf.c_str(), O_RDONLY)) < 0)
		_exit(-1, -1);
	if ((fd2 = open(_fileExec.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0777)) < 0)
		_exit(fd1, -1);

	if (dup2(fd1, STDIN_FILENO) < 0)
		_exit(fd1, fd2);
	if (dup2(fd2, STDOUT_FILENO) < 0)
		_exit(fd1, fd2);

	char	**envTab = _setEnv(fd1, fd2);

	std::string	pathDir = path.substr(0, path.find_last_of('/'));
	if (chdir(pathDir.c_str()) < 0)
	{
		_freeTab(envTab);
		_exit(fd1, fd2);
	}

	if (_root[pathDir.size()] == '/')
		_root.erase(0, pathDir.size() + 1);
	else
		_root.erase(0, pathDir.size());

	char *tab[2];
	tab[0] = strdup(path.substr(path.find_last_of('/') + 1).c_str());
	tab[1] = NULL;

	execve(path.substr(path.find_last_of('/') + 1).c_str(), tab, envTab);

	free(tab[0]);
	_freeTab(envTab);
	_exit(fd1, fd2);
}

bool	CgiHandler::_checkExtension(std::string path, std::set<std::string> ext)
{
	if (ext.count("ALL"))
		return true;

	size_t	i = path.rfind('.');
	if (i == std::string::npos)
		return false;
	else 
		return ext.count(path.substr(i));
}

void	CgiHandler::_exit(int fd1, int fd2)
{
	if (fd1 >= 0)
		close(fd1);
	if (fd2 >= 0)
		close(fd2);

	exit(EXIT_FAILURE);
}



/****************************************************/
/*                                                  */
/*                  PRIVATE SETTERS                 */
/*                                                  */
/****************************************************/
void	CgiHandler::setFilename()
{
	for (int i = 0; i < 2000; i++)
	{
		std::ostringstream	ss_filename;
		ss_filename << "/tmp/webserv_tmpFile_" << i;
		if (access(ss_filename.str().c_str(), F_OK) == 0)
			continue ;
		else
		{
			_fileBuf.assign(ss_filename.str());
			_fileExec.assign(_fileBuf + "_exec");
			_fs_file.open(_fileBuf.data(), std::fstream::out);
			_fs_file.close();
			return ;
		}
	}
	throw HttpError(500);
}

static std::string http_to_cgi_header(std::string const& s) {
	typedef std::string::size_type Sz;

	std::string res = "HTTP_";
	res.reserve(s.size());

	for (Sz i = 0; i < s.size(); ++i) {
		if (s[i] == '-')
			res.push_back('_');
		else
			res.push_back( toupper(s[i]) );
	}
	return res;
}

char	**CgiHandler::_setEnv(int fd1, int fd2)
{
	std::map<std::string, std::string>	env;
	char								**envTab;

	env["AUTH_TYPE"] = "";
	env["CONTENT_LENGTH"] = _fileSize();
	env["CONTENT_TYPE"] = _getFromHeader("content-type");
	env["GATEWAY_INTERFACE"] = "CGI/1.1";

	env["PATH_INFO"] = _getPathInfo();
	env["PATH_TRANSLATED"] 
		= canonize_path(_conf->translate_path(env["PATH_INFO"]));

	env["QUERY_STRING"] = (_getStrInfo(_pa.uri(), '?', 1) != "" ?
		_getStrInfo(_pa.uri(), '?', 1) : _getStrInfo(_pa.uri(), ';', 1));

	env["REMOTE_ADDR"] = "";
	env["REMOTE_HOST"] = _getFromHeader("host");

	env["REQUEST_METHOD"] = _method;
	env["SCRIPT_NAME"] = _getPath();
	env["SCRIPT_FILENAME"]
		= canonize_path(_conf->translate_path(env["SCRIPT_NAME"]));
	env["SERVER_NAME"] = _getStrInfo(_getFromHeader("host"), ':', 0);
	env["SERVER_PORT"] = _getStrInfo(_getFromHeader("host"), ':', 1);
	env["SERVER_PROTOCOL"] = "HTTP/1.1";
	env["SERVER_SOFTWARE"] = "spiderweb";

	typedef std::map<std::string, std::string>::const_iterator It;
	for (It it = _pa.header().begin(); it != _pa.header().end(); ++it) {
		if (it->first != "content-type")
			env[ http_to_cgi_header(it->first) ] = it->second;
	}
	int	i = 0;
	envTab = (char **)malloc(sizeof(char *) * (env.size() + 1));
	if (!envTab)
		_exit(fd1, fd2);
	for (std::map<std::string, std::string>::iterator it = env.begin(); it != env.end(); it++)
	{
		std::string	tmpStr = it->first + "=" + it->second;
		envTab[i] = (char *)malloc(sizeof(char) * (tmpStr.length() + 1));
		if (!envTab[i])
		{
			for (int j = 0; j < i; j++)
				free (envTab[j]);
			free (envTab);
			_exit(fd1, fd2);
		}
		strcpy(envTab[i], tmpStr.c_str());
		i++;
	}
	envTab[env.size()] = NULL;
	return (envTab);
}



/****************************************************/
/*                                                  */
/*                  ENV UTILS                       */
/*                                                  */
/****************************************************/
std::string	CgiHandler::_getFromHeader(std::string str)
{
	std::map<std::string, std::string>::iterator it = _header.find(str);
	if (it == _header.end())
		return ("");
	return (it->second);
}

std::string	CgiHandler::_getQueryStr() const
{
	size_t	pos = _pa.uri().find('?');
	if (pos == std::string::npos || pos + 1 == std::string::npos)
		return "";
	pos++;
	return (_pa.uri().substr(pos, _pa.uri().size() - pos));
}

std::string	CgiHandler::_getStrInfo(std::string str, char c, int method)
{
	if (str == "")
		return "";
	size_t	pos = str.find(c);
	if (method == 1)
	{
		if (pos == std::string::npos)
			return "";
		pos++;
		return (str.substr(pos, str.size() - pos));
	}
	if (pos == std::string::npos)
		return (str);
	return (str.substr(0, pos));
}

static bool	isDir(std::string path)
{
	DIR	*dir = opendir(path.c_str());
	if (dir)
	{
		closedir(dir);
		return true;
	}
	return false;
}

bool	CgiHandler::_fileExists(std::string path)
{
	if (isDir(path) == true)
		return false;
	if (access(path.c_str(), F_OK) < 0)
		return false;
	return true;
}

std::string	CgiHandler::_getPathInfo()
{
	std::string	path(_root);

	if (path.find('?') != std::string::npos)
		path.erase(path.find_first_of('?'), std::string::npos);
	if (path.find(';') != std::string::npos)
		path.erase(path.find_first_of(';'), std::string::npos);

	return (path.substr(_getPath().size()));
}

std::string	CgiHandler::_getPath()
{
	std::string	path(_root);

	if (path.find('?') != std::string::npos)
		path.erase(path.find_first_of('?'), std::string::npos);
	if (path.find(';') != std::string::npos)
		path.erase(path.find_first_of(';'), std::string::npos);

	if (_fileExists(path) == true)
		return (path);

	size_t i = 0;
	size_t j = 0;
	while (i != std::string::npos)
	{
		if (j == 0 && (path[0] == '/'))
			i = path.find_first_of('/', i);
		if (i == std::string::npos)
			return "";

		j = path.find_first_of('/', i + 1);
		if (j == std::string::npos)
			return "";
		else if (_fileExists(path.substr(0, j)) == true)
			return (path.substr(0, j));
		i++;
	}
	return "";
}

void	CgiHandler::_freeTab(char **envTab)
{
	if (!envTab)
		return ;
	for (size_t i = 0; envTab[i] != NULL; i++)
		free(envTab[i]);
	free(envTab);
	envTab = NULL;
}



/****************************************************/
/*                                                  */
/*                  PRINT                           */
/*                                                  */
/****************************************************/
void	CgiHandler::printEnv(char **envTab) const
{
	for (int i = 0; envTab[i] != NULL; i++)
		std::cerr << "Env: [" << envTab[i] << "]\n";
}

void	CgiHandler::printHeader() const
{
	std::map<std::string, std::string>	geader = _header;
	for (std::map<std::string, std::string>::iterator it = geader.begin(); it != geader.end(); it++)
		std::cerr << "Header = " << it->first << " - " << it->second << "\n";
}
