/* Our own header, to be included *after* all standard system headers */

#ifndef	__pass_fd_h
#define	__pass_fd_h

#include	<sys/types.h>	/* required for some of our prototypes */

void    setlogfile(char *);

ssize_t	readn(int, void *, size_t);
ssize_t	writen(int, const void *, size_t);

int	recv_fd(int);
int	send_fd(int, int);

#endif
