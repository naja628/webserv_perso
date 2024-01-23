## About
_webserv_ is simple, configurable HTTP/1.0 webserver implemented in C++.  
It is a school project of 19 School we worked on as a team with Matteo Bucci and Martin Delforge.  
This is my personal fork of it.  

## Build
On Unix systems just `make`.  
Windows not supported.

## Run
`./path/to/webserv_exe config_file`  
Or  
`./path/to/webserv` which is equivalent to `./path/to/webserv ./conf/server.conf` (server.conf is an example config provided)

The paths in the conf are relative and the default conf assumes you are running the server from the root of the repo.

After that you should be able to connect to `http://localhost:4242/`  
or `http://localhost:<port-you-configured>/`.  

Use 'Ctrl-C' to exit.

## Config
See [default conf](conf/server.conf) for an example config file, which sets the server to listen
on port 4242, execute `.py` cgi in `./assets/bin-cgi` using `/usr/bin/python3`,
sets up the `/dir` uri to be an autoindex of the contents of `./assets/www`,
and redirects 404 and 405 error to `./assets/error/bad.html`.
