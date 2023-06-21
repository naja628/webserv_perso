#ifndef QUIT_HPP
# define QUIT_HPP

class Quit {
private:
	static bool _quit;
public:
	static bool & set();
};

void sighook_quit(int sigcode);

#endif
