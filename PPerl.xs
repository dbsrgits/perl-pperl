#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "pass_fd.c"

MODULE = PPerl	PACKAGE = PPerl

PROTOTYPES: DISABLE

void
setlogfile(logfile)
    char *logfile;

int
s_pipe(in, out)
    SV *in;
    SV *out;
  PREINIT:
    int fd[2];
  CODE:
    RETVAL = s_pipe(fd);
    sv_setiv(in,  fd[0]);
    sv_setiv(out, fd[1]);
  OUTPUT:
    RETVAL

int
send_fd(over, this)
    int over;
    int this;
  OUTPUT: 
    RETVAL

int
recv_fd(on)
    int on;
  OUTPUT: 
    RETVAL

int
writen(fd, bytes, count)
     int fd;
     char *bytes;
     int count;
   OUTPUT:
     RETVAL

int
read_int(fd)
    int fd;
  PREINIT:
    int foo;
  CODE:
    read(fd, &foo, sizeof(foo));
    RETVAL = foo;
  OUTPUT:
    RETVAL
