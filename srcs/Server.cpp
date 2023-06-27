#include "Server.hpp"
#include "Quit.hpp"
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <fcntl.h>
#include <sstream>

#include "nat_utils.hpp"

Server::Server(VirtualServers const& conf) 
	: _m_NPorts(0), _m_conf(conf)
{}

Server::~Server() {
	for (size_t i = 0; i < _m_fds.size(); ++i) {
		close( _m_fds[i].fd );
	}
}

//Server Server::operator= (const Server& other)
//{
//	if (&other != this)
//	{
//		;
//	}
//	return *this;
//}

void Server::_m_setListen(const int port)
{
	int listener = socket(AF_INET, SOCK_STREAM, 0);	// port socket
	int sockopt = 1;

	if (listener == -1)
		throw SocketCreationException();

	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1)	// set socket option (ex: timeout)
		throw SocketCreationException();

	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(listener,(struct sockaddr*) &addr, sizeof(addr)) == -1)	// bind: put the socket on an address
		throw SocketBindingException();

	if (listen(listener, 4242) == -1)	// listen on the socket
		throw ConnectionListenException();

	_m_addFd(listener);	// add the socket in the vector
	++_m_NPorts;
}

void Server::launchServer(void)
{
	typedef std::set<int>::const_iterator It;
	for (It it = _m_conf.ports().begin(); it != _m_conf.ports().end(); ++it)
		_m_setListen(*it);
	_m_ports.assign(_m_conf.ports().begin(), _m_conf.ports().end());

	int events, cli_sock;
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_size = sizeof(cli_addr_size);

	std::clog << "Servers are ready\n";
	for (size_t i = 0; i < _m_conf.servers().size(); ++i) {
		_m_conf.servers()[i].info(std::clog);
	}

	while (1)
	{
		events = poll(_m_fds.data(), _m_fds.size(), 5000);	// poll: check for new connections
		if ( Quit::set() )
			return ;
		if (events == -1)
			throw PollErrorException();

		for (size_t current = 0; current != _m_NPorts; current++)
		{
			if (_m_fds[current].revents & POLLIN)
			{
				cli_sock = accept(_m_fds[current].fd, (struct sockaddr*) &cli_addr, &cli_addr_size);
				if (cli_sock == -1)
					continue;

				_m_addFd(cli_sock);
				_m_con_map[cli_sock].init(cli_sock, &_m_conf, _m_ports[current]);
			}
		}

		for (size_t current = _m_NPorts; current < _m_fds.size(); current++)
		{
			struct pollfd * cur = &_m_fds[current];
			if (cur->revents)
			{
				ConState& worker = _m_con_map[cur->fd];
				worker(cur->revents);
				cur->events = worker.event_set();
				if (!cur->events)
				{
					close(cur->fd);
					_m_con_map.erase(_m_con_map.find(cur->fd));
					std::clog << "Closed connection | id = " << cur->fd << "\n";
					_m_fds.erase(_m_fds.begin() + current--);
				}
				else 
					cur->events |= POLLIN; // to detect closes
			}
		}
	}
}

void Server::_m_addFd(const int sockfd)	// creates the struct for poll
{
	struct pollfd target;

	target.fd = sockfd;
	target.events = POLLIN;
	target.revents = 0;
	_m_fds.push_back(target);
}
