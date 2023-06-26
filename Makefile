NAME		=	webserv
CXX			=	c++
#CXXFLAGS	=	-Wall -Wextra -Werror -std=c++98 -I $(INCLUDES) 
CXXFLAGS	=	-Wall -Wextra -Werror -std=c++98 -I $(INCLUDES) -g3
OBJDIR		=	./objs/
SRCDIR		=	./srcs/
INCLUDES	=	./includes/

SRCS		=	main.cpp \
				Server.cpp	\
				HttpError.cpp \
				HttpParser.cpp \
				ConState.cpp \
				Buf.cpp \
				ChunkStreamer.cpp \
				Writer.cpp \
				Conf.cpp \
				Quit.cpp \
				CgiHandler.cpp \
				path_utils.cpp

OBJS		=	${addprefix $(OBJDIR), $(SRCS:%.cpp=%.o)}

# ==== #

$(OBJDIR)%.o:	$(SRCDIR)%.cpp
				$(CXX) $(CXXFLAGS) -c $< -o $@

all:			$(NAME)

$(NAME):		$(OBJDIR) $(OBJS)
				$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR):
				@mkdir -p $(OBJDIR)

clean:			purge
				rm -rf $(OBJDIR)

fclean:			clean
				rm -f $(NAME)

re: 			fclean $(NAME)

purge:
				rm -f /tmp/webserv_*

.PHONY: 		clean fclean re objs all purge
