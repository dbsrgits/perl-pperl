/* pperl - run perl scripts persistently */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include "pperl.h"

#define PREFORK 2
#define MAX_CLIENTS_PER_CHILD 3 /* ignored */

/* must never be less than 3 */
#define BUF_SIZE 4096

#define DEBUG 0

#ifdef ENOBUFS
#   define NO_BUFSPC(e) ((e) == ENOBUFS || (e) == ENOMEM)
#else
#   define NO_BUFSPC(e) ((e) == ENOMEM)
#endif

void Usage( char *pName );
void DecodeParm( char *pArg );
void DecodePPerlParm( char *pArg );
int  DispatchCall( char *scriptname, int argc, char **argv );
void DoIO( int sd, int errsd );
void ProcessError( int errsd );

char *pVersion = PPERL_VERSION;
char perl_options[1024];
extern char **environ;
int skreech_to_a_halt = 0;
int kill_script = 0;

#if DEBUG
void Debug( const char * format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
#else
void Debug()
{
}
#endif


int main( int argc, char **argv )
{
    int i;
    char *pArg;
    int pperl_section = 0;
    int return_code = 0;

    if( argc < 2 )
        Usage( argv[0] );

    for( i = 1; i<argc; i++ )
    {
        pArg = argv[i];
        if( *pArg == '-' )
        {
            if ( pArg[1] == '-' && pArg[2] == 0 ) {
                pperl_section++;
            }
            else {
                if (pperl_section) {
                    DecodePPerlParm( pArg );
                }
                else {
                    DecodeParm( pArg );
                }
            }
        }
        else
        {
            i++;
            return_code = DispatchCall( pArg, argc - i, (char**)(argv + i) );
            Debug("done\n");
            exit(return_code);
        }
    }

    return return_code;
}

void DecodePPerlParm( char *pArg )
{
	if ( (strncmp(pArg, "-k", 2) == 0) || (strncmp(pArg, "--kill", 6) == 0) ) {
		kill_script = 1;
	}
}

void DecodeParm( char *pArg )
{
    if ( (strlen(perl_options) + strlen(pArg) + 1) > 1000 ) {
        fprintf(stderr, "param list too long. Sorry.");
        exit(1);
    }
    strcat(perl_options, pArg);
    strcat(perl_options, " ");
}

void Usage( char *pName )
{
    printf( "pperl version %s\n", pVersion );

    if( pName == NULL )
    {
        printf( "Usage: pperl [perl options] [-- pperl options] filename\n" );
    }
    else
    {
        printf( "Usage: %.255s [perl options] [-- pperl options] filename\n", pName );
    }
    printf("perl options are passed to your perl executable.\n"
           "pperl options control the persistent perl behaviour\n"
		   "  currently the only pperl option is -k or --kill to kill the script\n"
		   "  being called\n");
    exit( 1 );
}

void *
my_malloc(size_t size)
{
    void *mem = malloc(size);
    if (mem == NULL) {
        perror("malloc failed");
        exit(-1);
    }
    return mem;
}

/* make socket name from scriptname, switching / for _ */
char *
MakeSockName(char * scriptname )
{
    char * sockname;
    char * save;
    char fullpath[PATH_MAX];
    int i = 0;

    realpath(scriptname, fullpath);
    /* Ugh. I am a terrible C programmer! */
    sockname = my_malloc(strlen(P_tmpdir) + strlen(fullpath) + 3);
    save = sockname;
    sprintf(sockname, "%s/", P_tmpdir);
    sockname += strlen(P_tmpdir) + 1;
    while (fullpath[i] != '\0') {
        if (fullpath[i] == '/') {
            *sockname = '_';
        }
        else if (fullpath[i] == '.') {
            *sockname = '_';
        }
        else {
            *sockname = fullpath[i];
        }
        sockname++; i++;
    }
    *sockname = '\0';
    return save;
}

void
sig_handler (int signal)
{
    skreech_to_a_halt++;
}

int GetReturnCode( char *sock_name )
{
    char buf[BUF_SIZE];
    int fd, readupto;
    ssize_t readlen;
    
    snprintf(buf, BUF_SIZE - 1, "%s.%d.ret", sock_name, getpid());
    Debug("Getting return code from: %s\n", buf);

    fd = open(buf, O_RDONLY);
    if (!fd) {
        perror("retcode open()");
    }
    unlink(buf);

    Debug("reading return code from %d\n", fd);
    buf[0] = '\0';
    readlen = read(fd, buf, BUF_SIZE - 1);
    if (readlen == -1) {
        perror("retcode read");
    }
    else {
        buf[readlen] = '\0';
    }

    Debug("return code is: %s\n", buf);

    close(fd);
    free(sock_name);
    return atoi(buf);
}

int DispatchCall( char *scriptname, int argc, char **argv )
{
    register int i, sd, errsd, len, sock_flags;
    ssize_t readlen;
    struct sockaddr_un saun;
    char *sock_name;
    char **env;
    char buf[BUF_SIZE];
	sd = 0;

    /* create socket name */
    sock_name = MakeSockName(scriptname);
    Debug("got socket: %s\n", sock_name);

    if (kill_script) {
	int pid_fd, sock_name_len;
	char *pid_file;
	pid_t pid;
	
	sock_name_len = strlen(sock_name);
	pid_file = my_malloc(sock_name_len + 5);
	strncpy(pid_file, sock_name, sock_name_len);
	pid_file[sock_name_len] = '.';
	pid_file[sock_name_len+1] = 'p';
	pid_file[sock_name_len+2] = 'i';
	pid_file[sock_name_len+3] = 'd';
	pid_file[sock_name_len+4] = '\0';
	
	Debug("opening pid_file: %s\n", pid_file);
	pid_fd = open(pid_file, O_RDONLY);
        if (pid_fd == -1) {
            Debug("Cannot open pid file (perhaps PPerl wasn't running for that script?)\n");
            write(1, "No process killed - no pid file\n", 32);
            return 0;
        }

	readlen = read(pid_fd, buf, BUF_SIZE);
	if (readlen == -1) {
            perror("nothing in pid file?");
            return 0;
	}
        buf[readlen] = '\0';

	close(pid_fd);

        pid = atoi(buf);
	Debug("got pid %d (%s)\n", pid, buf);
        if (kill(pid, SIGINT) == -1) {
            perror("could not kill process");
        }

	free(pid_file);

	return 0;
    }

    for (i = 0; i < 10; i++) {
        sd = socket(PF_UNIX, SOCK_STREAM, PF_UNSPEC);
        if (sd != -1) {
            break;
        }
        else if (NO_BUFSPC(errno)) {
            sleep(1);
        }
        else {
            perror("Couldn't create socket");
            return 1;
        }
    }

    saun.sun_family = PF_UNIX;
    strcpy(saun.sun_path, sock_name);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path) + 1;

    if (connect(sd, (struct sockaddr *)&saun, len) < 0) {
        /* Consider spawning Perl here and try again */
        int perl_script;
        int tmp_fd;
        char temp_file[BUF_SIZE];
        char syscall[BUF_SIZE];
        int i = 0;
        int pid, itmp, exitstatus;
        sigset_t mask, omask;

        buf[0] = 0;

        Debug("Couldn't connect, spawning new server: %s\n", strerror(errno));

        if (unlink(sock_name) != 0 && errno != ENOENT) {
            perror("removal of old socket failed");
            exit(1);
        }

        /* Create temp file with adjusted script... */
        perl_script = open(scriptname, O_RDONLY);
        if (perl_script == -1) {
            perror("Cannot open perl script");
            exit(1);
        }

        snprintf(temp_file, BUF_SIZE, "%s/%s", P_tmpdir, "pperlXXXXXX");
        tmp_fd = mkstemp(temp_file);
        if (tmp_fd == -1) {
            perror("Cannot create temporary file");
            exit(1);
        }

        write(tmp_fd, "### Temp File ###\n", 18);
        write(tmp_fd, perl_header, strlen(perl_header));

        /* output line number marker */
        write(tmp_fd, "\n#line 1 ", 9);
        write(tmp_fd, scriptname, strlen(scriptname));
        write(tmp_fd, "\n", 1);

        while( ( readlen = read(perl_script, buf, BUF_SIZE) ) ) {
            if (readlen == -1) {
                perror("read perl_script");
                exit(1);
            }
            write(tmp_fd, buf, readlen);
        }

        close(perl_script);

        write(tmp_fd, perl_footer, strlen(perl_footer));

        Debug("wrote file %s\n", temp_file);

        close(tmp_fd);

        /*** Temp file creation done ***/

        snprintf(syscall, BUF_SIZE, "%s %s %s %s %d %d", PERL_INTERP, perl_options, temp_file,
                sock_name, PREFORK, MAX_CLIENTS_PER_CHILD);
        Debug("syscall: %s\n", syscall);

        /* block SIGCHLD so noone else can wait() on the child before we do */
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &omask);

        if ((pid = system(syscall)) != 0) {
            unlink(temp_file);
            perror("perl script failed to start");
            exit(1);
        }
        Debug("waiting for perl to return...\n");
        while ((itmp = waitpid(0, &exitstatus, 0)) == -1 && errno == EINTR)
            ;
        sigprocmask(SIG_SETMASK, &omask, NULL);
        Debug("returned.\n");

        /* now remove the perl script */
        unlink(temp_file);

        /* try and connect to the new socket */
        while ((i++ <= 30) && (connect(sd, (struct sockaddr *)&saun, len) < 0))
        {
            Debug(".");
            sleep(1);
        }
        if (i >= 30) {
            perror("persistent perl process failed to start after 30 seconds");
            exit(1);
        }
        Debug("\n");
    }

    Debug("connected\n");

    /* print to socket... */
    send(sd, "[ENV]\n", 6, 0);
    
    for (i= 0, env = environ; *env; i++, env++); 
    snprintf(buf, BUF_SIZE, "%d\n", i);
    send(sd, buf, strlen(buf), 0);
    
    while ( *environ != NULL ) {
        size_t len = strlen(*environ) + 1;
        /* Debug("sending environ: %s\n", *environ); */
        send(sd, *environ, len, 0);
        environ++;
    }

    send(sd, "[CWD]\n", 6, 0);
    if (getcwd(buf, BUF_SIZE) == NULL) {
        perror("getcwd");
        exit (1);
    }
    send(sd, buf, strlen(buf), 0);
    send(sd, "\n", 1, 0);

    send(sd, "[ARGV]\n", 7, 0);
    snprintf(buf, BUF_SIZE, "%d\n", argc);
    send(sd, buf, strlen(buf), 0);
    for (i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        Debug("sending argv: %s\n", argv[i]);
        send(sd, argv[i], len, 0);
    }

    send(sd, "[PID]\n", 6, 0);
    snprintf(buf, BUF_SIZE, "%d\n", getpid());
    send(sd, buf, strlen(buf), 0);
    
    send(sd, "[STDIO]\n", 8, 0);

    Debug("waiting for OK message\n");
    if (recv(sd, buf, 3, 0) != 3) {
        perror("failed to get OK message");
        exit(1);
    }
    if (strncmp(buf, "OK\n", 3)) {
        buf[3] = 0;
        fprintf(stderr, "incorrect ok message: %s\n", buf);
        exit(1);
    }

    /* open stderr stream */
    snprintf(buf, BUF_SIZE, "%s.%d.err", sock_name, getpid());
    Debug("openning STDERR: %s\n", buf);
    if ( (errsd = open(buf, O_RDONLY)) == -1) {
        perror("open STDERR");
        exit(1);
    }

    /* set flags */
    sock_flags = fcntl(sd, F_GETFL);
    fcntl(sd, F_SETFL, sock_flags | O_NONBLOCK);

    /* set flags */
    sock_flags = fcntl(errsd, F_GETFL);
    fcntl(errsd, F_SETFL, sock_flags | O_NONBLOCK);

    Debug("reading results\n");
    DoIO(sd, errsd);
    
    Debug("cleanup, deleting %s\n", buf);
    unlink(buf);
    close(errsd);

    Debug("exiting\n");

    return GetReturnCode(sock_name);
}

void
ProcessError( int errsd )
{
    char buf[BUF_SIZE];
    ssize_t readlen;

    Debug("reading stderr\n");
    readlen = read(errsd, buf, BUF_SIZE);
    if (readlen == -1) {
        /* might be EAGAIN (deadlock). Try writing instead */
        if (errno != EAGAIN) {
            perror("ProcessError read");
        }
    }
    else {
        /* Debug(buf); */
        write(2, buf, readlen);
    }
}


void
DoIO( int sd, int errsd )
{
    fd_set rfds, wfds, stdin_fds;
    struct timeval tv;
    int maybe_read, maybe_write;
    ssize_t readlen, writelen;
    char buf[BUF_SIZE];
    int allow_writes = 1;        

    maybe_read = maybe_write = 1;

   /* set stdin to non-blocking */
    /* sock_flags = fcntl(0, F_GETFL); 
       fcntl(0, F_SETFL, sock_flags | O_NONBLOCK); 
    */

    signal(SIGINT, sig_handler);

    /* while we're connected... */
    while (!skreech_to_a_halt) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);

        FD_ZERO(&stdin_fds);
        FD_SET(0, &stdin_fds);

        /* for some reason, if I make usec == 0 (poll), performance sucks */
        tv.tv_sec = 0;
        tv.tv_usec = 1;

        Debug("checking for read...\n");

        if (select(sd + 1, &rfds, NULL, NULL, &tv)) {
            /* can read */
            Debug("can read...\n");
            if (FD_ISSET(sd, &rfds)) {
                Debug("reading stdout\n");
                readlen = read(sd, buf, BUF_SIZE);
                if (readlen == -1) {
                    /* might be EAGAIN (deadlock). Try writing instead */
                    if (errno != EAGAIN) {
                        perror("read stdout");
                    }
                    continue;
                }
                if (readlen == 0) {
                    Debug("all done. remote end closed socket\n");
                    shutdown(sd, 2);
                    close(sd);
                    skreech_to_a_halt = 1;
                    /* fflush(NULL); eh? */
                    continue;
                }
                /* Debug(buf); */
                writelen = write(1, buf, readlen);
                if (writelen != readlen) {
                    perror("bar");
                    break;
                }
            }
        }

        Debug("checking for stdin/write...\n");

        tv.tv_sec = 0;
        tv.tv_usec = 1;

        if ( (allow_writes == 1) && select(1, &stdin_fds, NULL, NULL, &tv)) {
            Debug("something to send\n");
            /* something on stdin to send */
            tv.tv_sec = 0;
            tv.tv_usec = 1;

            FD_ZERO(&wfds);
            FD_SET(sd, &wfds);

            tv.tv_sec = 0;
            tv.tv_usec = 0;
            readlen = read(0, buf, BUF_SIZE);
            Debug("read done, read %d bytes\n", readlen);
            if (readlen < 1) {
                /* ignore STDIN read errors and closed STDIN */
                if (errno != EAGAIN) {
                    /* perror("stdin"); */
                    Debug("shutting down write part\n");
                    fflush(NULL);
                    shutdown(sd, 1); /* close writing part of the socket now */
                    allow_writes = 0;
                }
            }
            else {
                buf[readlen] = 0;
                Debug("send: %s onto %d\n", buf, sd);
                send(sd, buf, readlen, 0);
                Debug("sent\n");
            }
        }

        ProcessError(errsd);
    }
    
    ProcessError(errsd);

    if (skreech_to_a_halt) {
        /* perror("closing socket"); */
        shutdown(sd, 2);
        close(sd);
        fflush(NULL);
        return;
    }

    return;

}


/* 
Local Variables:
mode: C
c-basic-offset: 4
tab-width: 4
indent-tabs-mode: nil
End:
*/
