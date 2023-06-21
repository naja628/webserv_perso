#include <signal.h>

#include <fstream>
#include <iostream>
#include "webserv.hpp"
#include "Conf.hpp"

#ifndef DFL_CONF
#define DFL_CONF "conf/server.conf"
#endif

int main(int ac, char **av)
{
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
 
 	try {
 		// parse config
 		VirtualServers servers(conf_stream);
 		conf_stream.close();
 
 		// launch server
 		Server serv(servers);
 		serv.launchServer();
 	} catch (ParsingException const& e) {
 		e.print_error(std::cerr);
 		return 1;
 	}
 // 	} catch ( HttpError e) {
 // 		std::cerr << "Error: " << e << "\n";
 // 		return 1;
 // 	} catch (...) {
 // 		std::cerr << "Error\n"; 
 // 		return 1;
 // 	}
 

	return 0;
}

