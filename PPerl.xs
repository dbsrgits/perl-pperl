#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "pass_fd.c"

MODULE = PPerl	PACKAGE = PPerl

PROTOTYPES: DISABLE

void
setreadonly(char *name, int value)
  CODE:
    GV *tmpgv;
    if ((tmpgv = gv_fetchpv(name, TRUE, SVt_PV))) {
        SvREADONLY_off(GvSV(tmpgv));
	sv_setiv(GvSV(tmpgv), value);
        SvREADONLY_on(GvSV(tmpgv));
    }


int
s_pipe(SV* in, SV* out)
  CODE:
    int fd[2];

    RETVAL = s_pipe(fd);
    sv_setiv(in,  fd[0]);
    sv_setiv(out, fd[1]);
  OUTPUT:
    RETVAL


int
send_fd(int over, int this)


int
recv_fd(int on)


int
read_int(int fd)
  CODE:
    int foo;

    read(fd, &foo, sizeof(foo));
    RETVAL = foo;
  OUTPUT:
    RETVAL
