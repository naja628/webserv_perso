/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mbucci <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/04/24 16:39:14 by mbucci            #+#    #+#             */
/*   Updated: 2023/06/26 17:10:51 by mdelforg         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <vector>
#include <exception>
#include <poll.h>
#include <map>
#include "HttpParser.hpp"
#include "ConState.hpp"
#include "Conf.hpp"

class Server
{
	public:
		Server(VirtualServers const& conf);
		// TODO: add config constructor
		virtual ~Server();

		void launchServer();

		class SocketCreationException : public std::exception
		{
			const char* what() const throw()
			{
				return "could not create socket.";
			}
		};

		class SocketBindingException : public std::exception
		{
			const char* what() const throw()
			{
				return "could not bind socket.";
			}
		};

		class ConnectionListenException : public std::exception
		{
			const char* what() const throw()
			{
				return "could not listen through socket.";
			}
		};

		class ConnectionAcceptException : public std::exception
		{
			const char* what() const throw()
			{
				return "could not accept new connection.";
			}
		};

		class PollErrorException : public std::exception
		{
			const char* what() const throw()
			{
				return "poll error.";
			}
		};

	private:
		Server(const Server& other);	// deactivate
		Server operator= (const Server& other);	// deactivate

		// config oject;
		size_t _m_NPorts;
		std::vector<struct pollfd> _m_fds;
		std::vector<int> _m_ports;

		std::map<int, ConState> _m_con_map;
		VirtualServers _m_conf;

		void _m_handleRequest(const int sock, const HttpParser& pa);
		void _m_addFd(const int sockfd);
		void _m_setListen(const int port);
		std::string	_convertAddr(struct sockaddr_in cli_addr);
};
