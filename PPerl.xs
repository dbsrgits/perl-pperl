#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "pass_fd.c"

ssize_t
dummy_callback(int foo, const void *bar, size_t baz)
{ return -1; }

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
dup2(old, new)
     int old;
     int new;
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
close_fd(fd)
    int fd;
  CODE:
    RETVAL = close(fd);
  OUTPUT:
    RETVAL

char
read_byte(fd)
    int fd;
  PREINIT:
    char foo;
  CODE:
    read(fd, &foo, 1);
    RETVAL = foo;
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