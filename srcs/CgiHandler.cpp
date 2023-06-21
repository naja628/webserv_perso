#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "Buf.hpp"
#include "HttpParser.hpp"
#include "Conf.hpp"

/****************************************************/
/*                                                  */
/*                  CONSTRUCTORS                    */
/*                                                  */
/****************************************************/
// default
CgiHandler::CgiHandler() : _envTab(NULL)
{}

// copy
CgiHandler::CgiHandler(const CgiHandler &src)
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
	std::cerr << "cgi dtor" << std::endl;
	rmFile();
	if (_envTab) 
		_freeTab();
}


/****************************************************/
/*                                                  */
/*                  ASSIGN OPERATOR                 */
/*                                                  */
/****************************************************/
CgiHandler	&CgiHandler::operator=(const CgiHandler &src)
{
	_pa = src._pa;
	_env = src._env;
	_header = src._header;
	_method = src._method;
	_fileBuf = src._fileBuf;
	_fileExec = src._fileExec;
	_root = src._root;
	_header = src._header;
	_env = src._env;
	_envTab = src._envTab;
	// problem in general case for `_envTab`, `_fs_file`
	return (*this);
}


/****************************************************/
/*                                                  */
/*                  GETTERS                         */
/*                                                  */
/****************************************************/
std::string	CgiHandler::getMethod() const
{
	return (_method);
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



/****************************************************/
/*                                                  */
/*                  FILES OPERATIONS                */
/*                                                  */
/****************************************************/
// filestream
// close le stream avant d'execve
// (open + trunc) si 1er appel, sinon (open + append) / ou bien rm apres execve et direct recreer un
bool	CgiHandler::openFile(int method)
{
	if (method == 0)
	{
// 		std::cerr << "file buf: " << _fileBuf << "\n";
		_fs_file.open(_fileBuf.data(), std::fstream::out | std::fstream::trunc);
		if (!_fs_file.is_open())
			return 1;	// error
// 		std::cerr << "after open for write\n";
// 		std::cerr << "good? : " << _fs_file.good() << ", ";
// 		std::cerr << "fail? : " << _fs_file.fail() << ", ";
// 		std::cerr << "eof? : " << _fs_file.eof() << ", ";
// 		std::cerr << "bad? : " << _fs_file.bad() << "\n";
	}
	else
	{
		_fs_file.open(_fileBuf.data(), std::fstream::in);
		if (!_fs_file.is_open())
			return 1;	// error
	}

	return 0;
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
// 
// 	std::ios_base::iostate flags = _fs_file.rdstate();
// 	std::cerr << flags << "\n";
// 	std::cerr << "good? : " << _fs_file.good() << ", ";
// 	std::cerr << "fail? : " << _fs_file.fail() << ", ";
// 	std::cerr << "eof? : " << _fs_file.eof() << ", ";
// 	std::cerr << "bad? : " << _fs_file.bad() << "\n";


	std::streampos	startPos = _fs_file.tellg();
	_fs_file.write(buf.pos, std::min(rem_chunk_size, data_size));
// 	std::cout.write(buf.pos, std::min(rem_chunk_size, data_size));
	std::streampos	endPos = _fs_file.tellg();

// 	flags = _fs_file.rdstate();
// 	std::cerr << flags << "\n";
// 	std::cerr << "good? : " << _fs_file.good() << ", ";
// 	std::cerr << "fail? : " << _fs_file.fail() << ", ";
// 	std::cerr << "eof? : " << _fs_file.eof() << ", ";
// 	std::cerr << "bad? : " << _fs_file.bad() << "\n";
// 
// 
// 	std::cerr << "##### wrote to fstream : " << endPos - startPos << "bytes\n";
	return (endPos - startPos);
}

std::string	CgiHandler::_fileSize() // has to be opened
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

void	CgiHandler::clearDirectory(std::string dirPath)
{
	struct dirent	*ent;

	DIR	*dir = opendir(dirPath.c_str());
	if (dir == NULL)
		return ;	// error

	while ((ent = readdir(dir)) != NULL)
		std::remove((dirPath + ent->d_name).c_str());
	closedir(dir);
}



/****************************************************/
/*                                                  */
/*                  LAUNCH                          */
/*                                                  */
/****************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/*
std::string	CgiHandler::run()
{
std::cerr << "_in_cgi\n";

	int	timeout = 5;
	int	kill_signal = SIGKILL;

    pid_t intermediate_pid = fork();
	if(intermediate_pid < 0)
		return NULL;	// error
	if (intermediate_pid == 0)
	{
		pid_t worker_pid = fork();
		if (worker_pid < 0)
			return NULL;	// error
		if (worker_pid == 0)
			_launch();

		pid_t timeout_pid = fork();
		if (timeout_pid == 0)
		{
			sleep(timeout);
			exit(0);
		}

		pid_t exited_pid = wait(NULL);
		if (exited_pid == worker_pid)
			kill(timeout_pid, SIGKILL);
		else
			kill(worker_pid, kill_signal);

		wait(NULL); // Collect the other process	// convert waitpid
		exit(0); // Or some more informative status
	}

	int	wstatus;
	waitpid(intermediate_pid, &wstatus, 0);	// change '0'
std::cerr << "------> wstatus = " << WEXITSTATUS(wstatus) << '\n';

	if (wstatus)
		return NULL;	// error

	return (_fileToStr());
}
*/

std::string	CgiHandler::run()
{
std::cerr << "_in_cgi\n";
	int pid;

	pid = fork();
	if (pid < 0)
		return NULL;	// error
	if (!pid)
		_launch();

	int	wstatus;
	waitpid(pid, &wstatus, 0);	// change '0'	// WNOHANG
std::cerr << "------> wstatus = " << WEXITSTATUS(wstatus) << '\n';
	if (wstatus)
		return "";	// error

	return (_fileToStr());
}

bool	CgiHandler::isCgi()
{
	std::string	path = _getPath(0);
std::cerr << "isCgi -> path = " << path << '\n';
	if (path == "")
		return false;

	return (_checkExtension(path));
}



/****************************************************/
/*                                                  */
/*                  LAUNCH UTILS                    */
/*                                                  */
/****************************************************/
void	CgiHandler::_launch()
{
	int	fd1, fd2;

	std::string	path = _getPath(0);
// 	if (access(path.c_str(), F_OK) == -1)	// avant	// necessaire? comme on verifie dans isCgi()
	if (access(path.c_str(), X_OK) == -1)	// avant	// necessaire? comme on verifie dans isCgi()
		_exit(-1, -1);

	if ((fd1 = open(_fileBuf.c_str(), O_RDONLY)) < 0)
		_exit(-1, -1);
	if ((fd2 = open(_fileExec.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0777)) < 0)
		_exit(fd1, -1);

	if (dup2(fd1, STDIN_FILENO) < 0)
		_exit(fd1, fd2);
	if (dup2(fd2, STDOUT_FILENO) < 0)
		_exit(fd1, fd2);

	if (chdir(path.substr(0, path.find_last_of('/')).c_str()) < 0)	// check if good path
	{
std::cerr << "Couldn't change directory\n";
		_exit(fd1, fd2);
	}

	_setEnv(fd1, fd2);	// close les fd

	char *tab[2];
	tab[0] = (char *)path.substr(path.find_last_of('/') + 1).c_str();
	tab[1] = NULL;

std::cerr << "-> exec to: " << tab[0] << '\n';
	execve(path.substr(path.find_last_of('/') + 1).c_str(), tab, _envTab);	// change path, because of chdir()	// path.c_str()
std::cerr << "-> couldn't execute\n\n";

	_freeTab();
	_exit(fd1, fd2);
}

bool	CgiHandler::_checkExtension(std::string path)
{
	size_t	i = path.find_last_of('.');

	if (i != std::string::npos && (path.substr(i) == ".py" || path.substr(i) == ".php"))
		return true;

	return false;
}

std::string	CgiHandler::_fileToStr()
{
	std::ifstream		ifs(_fileExec.data());
	std::stringstream	buf;

	buf << ifs.rdbuf();
	return (buf.str());
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
bool	CgiHandler::setFilename()	// throw a la place de return
{
	for (int i = 0; i < 100; i++)	// > 100
	{
		std::ostringstream	ss_filename;
		ss_filename << "runtime/tmpFile_" << i;
		if (FILE *file = std::fopen(ss_filename.str().c_str(), "r"))
			fclose(file);
		else
		{
			_fileBuf.assign(ss_filename.str());
			_fileExec.assign(_fileBuf + "_exec");	// dans un setters
			_fs_file.open(_fileBuf.data(), std::fstream::out);	// est-ce qu'on cree file_exec ici
			_fs_file.close();
			return 0;
		}
	}
	return 1;
}

void	CgiHandler::_setEnv(int fd1, int fd2)	// traduire les "./"	// exit a la place de return
{
	_env["AUTH_TYPE"] = "";
	_env["CONTENT_LENGTH"] = _fileSize();
	_env["CONTENT_TYPE"] = _getFromHeader("content-type");
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";

	_env["PATH_INFO"] = _getPathInfo();
	_env["PATH_TRANSLATED"] = "";	// locPath

	_env["QUERY_STRING"] = (_getStrInfo(_pa.uri(), '?', 1) != "" ?
		_getStrInfo(_pa.uri(), '?', 1) : _getStrInfo(_pa.uri(), ';', 1));

	_env["REMOTE_ADDR"] = "";	// from cli_sock in Server.cpp
	_env["REMOTE_HOST"] = _getFromHeader("host");	// localhost:port

	_env["REQUEST_METHOD"] = _method;
	_env["SCRIPT_NAME"] = _getPath(1);	// /cgi-bin/process.php	// problem while print (".")
	_env["SCRIPT_FILENAME"] = _getPath(0);	// _root + getPath();	// root + script_name
	_env["SERVER_NAME"] = _getStrInfo(_getFromHeader("host"), ':', 0);
	_env["SERVER_PORT"] = _getStrInfo(_getFromHeader("host"), ':', 1);
	_env["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env["SERVER_SOFTWARE"] = "spiderweb";	// spiderweb

	int	i = 0;
	_envTab = (char **)malloc(sizeof(char *) * (_env.size() + 1));
	if (!_envTab)
		_exit(fd1, fd2);
	for (std::map<std::string, std::string>::iterator it = _env.begin(); it != _env.end(); it++)
	{
		std::string	tmpStr = it->first + "=" + it->second;
		_envTab[i] = (char *)malloc(sizeof(char) * (tmpStr.length() + 1));
		if (!_envTab[i])
		{
			for (int j = 0; j < i; j++)
				free (_envTab[j]);
			free (_envTab);
			_exit(fd1, fd2);
		}
		strcpy(_envTab[i], tmpStr.c_str());
		i++;
	}
	_envTab[_env.size()] = NULL;
std::cerr << "env created\n";
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

bool	CgiHandler::_fileExists(std::string path)
{
	struct stat	fileStat;

	if (stat(path.c_str(), &fileStat) == 0)
		if (S_ISREG(fileStat.st_mode))
			return true;
	return false;
}

std::string	CgiHandler::_getPathInfo()
{
	std::string	path(_root);	// hardcode path
	path += _pa.uri();

	if (path.find('?') != std::string::npos)
		path.erase(path.find_first_of('?'), std::string::npos);
	if (path.find(';') != std::string::npos)
		path.erase(path.find_first_of(';'), std::string::npos);

	size_t	i = 0;
	size_t	j;

	while (i != std::string::npos)
	{
		i = path.find_first_of('/', i);
		if (i == std::string::npos)
			return "";

		j = path.find_first_of('/', i + 1);
		if (j != std::string::npos)
		{
			if (_fileExists(path.substr(0, j)) == true)
				return (path.substr(j, path.size() - j));
		}
		i++;
	}
	return "";
}

std::string	CgiHandler::_getPath(int method)
{
	std::string	path(_root);	// hardcode path

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
		i = path.find_first_of('/', i);
		if (i == std::string::npos)
			return "";

		j = path.find_first_of('/', i + 1);
		if (j == std::string::npos)
			return "";
		else if (_fileExists(path.substr(0, j)) == true)
		{
			if (method == 0)
				return (path.substr(0, j));
			else
				return (path.substr(_root.size(), j));
		}
		i++;
	}
	return "";
}

void	CgiHandler::_freeTab()
{
	if (!_envTab)
		return ;
	for (size_t i = 0; i < _env.size(); i++) {
		std::cerr << "i : " << i << std::endl;
		free(_envTab[i]);
	}
	free(_envTab);
	_envTab = NULL;
}



/****************************************************/
/*                                                  */
/*                  PRINT                           */
/*                                                  */
/****************************************************/
void	CgiHandler::printEnv() const
{
//	std::map<std::string, std::string>	geader = _env;
//	for (std::map<std::string, std::string>::iterator it = geader.begin(); it != geader.end(); it++)
//		std::cerr << "Env = " << it->first << " - [" << it->second << "]\n";
	for (int i = 0; _envTab[i] != NULL; i++)
		std::cerr << "Env: [" << _envTab[i] << "]\n";
}

void	CgiHandler::printHeader() const
{
	std::map<std::string, std::string>	geader = _header;
	for (std::map<std::string, std::string>::iterator it = geader.begin(); it != geader.end(); it++)
		std::cerr << "Header = " << it->first << " - " << it->second << "\n";
}



