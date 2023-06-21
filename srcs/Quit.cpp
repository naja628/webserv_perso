#include <cstdlib>
#include "Quit.hpp"

bool Quit::_quit = false;
bool & Quit::set() {
	return _quit;
}

void sighook_quit(int sigcode) {
	if ( Quit::set() )
		exit(128 + sigcode);

	Quit::set() = true;
}

