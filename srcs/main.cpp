#include <signal.h>
#include <fstream>
#include <iostream>
#include "webserv.hpp"
#include "Conf.hpp"
#include "Quit.hpp"
#include "path_utils.hpp"

#ifndef DFL_CONF
#define DFL_CONF "conf/server.conf"
#endif

int main(int ac, char **av, char ** env)
{
	if (!bad_setcwd(env)) {
		std::cerr 
			<< "Warning: couldn't find environment variable `PWD`.\n"
			<< "Cgi scripts may use wrong paths when config uses relative paths.\n"
			<< "Consider exporting it manually\n";
	} 
	else // DEBUG
		std::cerr << "cwd: " << bad_getcwd() << "\n";
	
	signal(SIGINT, sighook_quit);
 	// basic checks
 	std::ifstream conf_stream;
 	if (ac < 2)
 	{
 		(void)av;
 		conf_stream.open(DFL_CONF);
 	}
 	else if (ac == 2)
 	{
 		conf_stream.open(av[1]);
 	} else {
 		std::cerr << "Usage: " << av[0] << " [conf_file]\n";
 		return 1;
 	}
 
	VirtualServers servers;
 	try {
 		// parse config
		servers.parse_conf(conf_stream);
 		conf_stream.close();
 	} catch (ParsingException const& e) {
 		e.print_error(std::cerr);
 		return 1;
 	}

	while (1)
	{
		// launch server
		try {
			Server serv(servers);
			serv.launchServer();
			std::cerr << "normal exit" << std::endl;
			return 0;
		}
		catch (const std::bad_alloc& e)
		{
			std::cerr << "in bad alloc\n";
		}
		catch (const std::exception& e)
		{
			std::cerr << "Fatal error: " << e.what() << std::endl;
			return 1;
		}
	}

	return 0;
}

