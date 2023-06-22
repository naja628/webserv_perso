#ifndef _CGI_HANDLER_HPP_
# define _CGI_HANDLER_HPP_

# include <vector>
# include <map>
# include <fstream>
# include <ctime>

# include "Buf.hpp"
# include "HttpParser.hpp"
# include "Conf.hpp"

# define CGI_TIMEOUT 0.3

class CgiHandler
{

	public:


	// constructors
		CgiHandler();
		CgiHandler(const CgiHandler &src);

	// destructor
		~CgiHandler();

	// assign operator
		CgiHandler	&operator=(const CgiHandler &src);

	// setters
		void	setFilename();
		void	setRoot(std::string root);
		void	setParser(HttpParser pa);
		void	setMethod(std::string method);
		void	setConf(const ServerConf *pconf);

	// files operators
		void		openFile(int method);	// dispatch
		void		closeFile();
		void		rmFile();
		ssize_t		bodyInFile(Buf & buf, size_t rem_chunk_size);
		void		clearDirectory(std::string dirPath);
		std::string	fileToStr();

	// launch
		bool	isCgi();
		void	run();
		bool	waitChild();

	// print		// debug only
		void	printHeader() const;
		void	printEnv(char **envTab) const;


	private:

	// variables
		std::string							_method;	// set dans ConState
		std::string							_fileBuf;	// full path
		std::string							_fileExec;	// fileBuf + "_exec"
		std::string							_root;	// pathConf(getter)
		std::fstream						_fs_file;
		std::map<std::string, std::string>	_header;
		
		int		_pid;
		clock_t	_stopwatch;

		HttpParser							_pa;

	// launch utils
		void		_launch();
		bool		_checkExtension(std::string path);
		void		_exit(int fd1, int fd2);

	// setters
		char		**_setEnv(int fd1, int fd2);
		
	// env utils
		std::string	_fileSize();
		std::string	_getFromHeader(std::string str);
		std::string	_getQueryStr() const;
		std::string	_getStrInfo(std::string str, char c, int method);
		bool		_fileExists(std::string path);
		std::string	_getPathInfo();
		std::string	_getPath(int method);
		void		_freeTab(char **envTab);

};

#endif
