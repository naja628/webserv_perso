#include <signal.h>

#include <fstream>
#include <iostream>
#include "webserv.hpp"
#include "Conf.hpp"

void sigpipe(int ign) {
	(void) ign;
	std::cerr << "handling SIGPIPE\n";
}

struct sig_throw {};
static void exit_scope(int ign) {
// 	std::cerr << "handling sigint" << std::endl;
	(void) ign;
	throw sig_throw();
}

#define DFL_CONF "conf/server.conf"
static int do_the_thing(int ac, char ** av) {
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

	return 1; // impossible path?

}

int main(int ac, char **av)
{
// 	signal(SIGPIPE, SIG_IGN);
// 	signal(SIGPIPE, sigpipe);
	signal(SIGINT, exit_scope);
	int exit_code;
	try {
		exit_code = do_the_thing(ac, av);
	} catch (sig_throw stateless) {
		std::cerr << "foobang" << std::endl;
		exit(130);
	}
	return exit_code;
// 	// basic checks
// 	std::ifstream conf_stream;
// 	if (ac < 2)
// 	{
// 		(void)av;
// 		conf_stream.open(DFL_CONF);
// 	}
// 	else if (ac == 2)
// 	{
// 		conf_stream.open(av[1]);
// 	} else {
// 		std::cerr << "Usage: " << av[0] << " [conf_file]\n";
// 		return 1;
// 	}
// 
// 	try {
// 		// parse config
// 		VirtualServers servers(conf_stream);
// 		conf_stream.close();
// 
// 		// launch server
// 		Server serv(servers);
// 		serv.launchServer();
// 	} catch (ParsingException const& e) {
// 		e.print_error(std::cerr);
// 		return 1;
// 	}
// // 	} catch ( HttpError e) {
// // 		std::cerr << "Error: " << e << "\n";
// // 		return 1;
// // 	} catch (...) {
// // 		std::cerr << "Error\n"; 
// // 		return 1;
// // 	}
// 

	return 0;
}

