//pperl - run perl scripts persistently

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include "pperl.h"

#define PREFORK 1
#define MAX_CLIENTS_PER_CHILD 2

#define DEBUG 0

#ifdef ENOBUFS
#   define NO_BUFSPC(e) ((e) == ENOBUFS || (e) == ENOMEM)
#else
#   define NO_BUFSPC(e) ((e) == ENOMEM)
#endif

void Usage( char *pName );
void DecodeParm( char *pArg );
void DecodePPerlParm( char *pArg );
void DispatchCall( char *scriptname, int argc, char **argv );
void DoIO( int sd, int errsd );

char *pVersion = "0.02";
char perl_options[1024];
extern char **environ;
int skreech_to_a_halt = 0;

void Debug( const char * format, ...)
{
    va_list args;
    
#if !DEBUG
    return;
#endif
    
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void send_escaped_line( int sd, char * value )
{
    char new_val[4096];
    int i = 0;
    
    while( *value )
    {
        switch (*value)
        {
            case '\n':
                new_val[i++] = '%';
                new_val[i++] = '0';
                new_val[i++] = 'a';
                break;
            default:
                new_val[i++] = *value;
        }
        *value++;
    }
    new_val[i] = '\0';
    
    send(sd, new_val, strlen(new_val), 0);
    send(sd, "\n", 1, 0);
}

int main( int argc, char **argv )
{
    int i;
    char *pArg;
    int pperl_section = 0;

    if( argc < 2 )
        Usage( argv[0] );

    for( i = 1; i<argc; i++ )
    {
        pArg = argv[i];
        if( *pArg == '-' )
        {
            if ( pArg[1] == '-' ) {
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
            DispatchCall( pArg, argc - i, (char**)(argv + i) );
            Debug("done\n");
            exit(0);
        }
    }

    return 0;
}

void DecodePPerlParm( char *pArg )
{
    
}

void DecodeParm( char *pArg )
{
    size_t length;
    
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
           "pperl options control the persistent perl behaviour\n");
    exit( 1 );
}

/* make socket name from scriptname, switching / for _ */
char *
MakeSockName(char * scriptname, int error )
{
    char * sockname;
    char * save;
    char fullpath[PATH_MAX];
    int i = 0;
    int socknamelen;
    
    realpath(scriptname, fullpath);
    /* Ugh. I am a terrible C programmer! */
    socknamelen = strlen(P_tmpdir) + strlen(fullpath) + 3 + (error * 4);
    sockname = malloc(socknamelen);
    save = sockname;
    sprintf(sockname, "%s/", P_tmpdir);
    sockname += strlen(P_tmpdir) + 1;
    while (fullpath[i] != '\0') {
        if (fullpath[i] == '/') {
            *sockname = '_';
        }
        else if (fullpath[i] == '.') {
            *sockname = ',';
        }
        else {
            *sockname = fullpath[i];
        }
        *sockname++; i++;
    }
    if (error) {
        *sockname = '.'; *sockname++;
        *sockname = 'e'; *sockname++;
        *sockname = 'r'; *sockname++;
        *sockname = 'r'; *sockname++;
    }
    *sockname = '\0';
    return save;
}

void
sig_handler (int signal)
{
    skreech_to_a_halt++;
}

void DispatchCall( char *scriptname, int argc, char **argv )
{
    register int i, sd, errsd, len, sock_flags;
    struct sockaddr_un saun;
    char *sock_name;
    char buf[1024];
    int readlen;
    char c;
    
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
            exit(1);
        }
    }
    
    /* create socket name */
    sock_name = MakeSockName(scriptname, 0);
    Debug("got socket: %s\n", sock_name);
    
    saun.sun_family = PF_UNIX;
    strcpy(saun.sun_path, sock_name);
    
    len = sizeof(saun.sun_family) + strlen(saun.sun_path) + 1;
    
    if (connect(sd, (struct sockaddr *)&saun, len) < 0) {
        /* Consider spawning Perl here and try again */
        int perl_script;
        int tmp_fd;
        char temp_file[1024];
        char syscall[4096];
        int i = 0;
        int pid, itmp, exitstatus;
        sigset_t mask, omask;
        
        buf[0] = 0;
        
        Debug("Couldn't connect, spawning new server: %s", strerror(errno));
        
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
        
        snprintf(temp_file, 1024, "%s/%s", P_tmpdir, "pperlXXXXXX");
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
        
        while( readlen = read(perl_script, &buf, 500) ) {
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
        
        snprintf(syscall, 4096, "%s %s %s %s %d %d", PERL_INTERP, perl_options, temp_file,
                sock_name, PREFORK, MAX_CLIENTS_PER_CHILD);
        Debug("syscall: %s\n", syscall);
        
        // block SIGCHLD so noone else can wait() on the child before we do
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
        
        // now remove the perl script
        unlink(temp_file);
        
        // try and connect to the new socket
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
    while ( *environ != NULL ) {
        // Debug("sending environ: %s\n", *environ);
        send_escaped_line(sd, *environ);
        *environ++;
    }
    
    send(sd, "[ARGV]\n", 7, 0);
    for (i = 0; i < argc; i++) {
        // Debug("sending argv: %s\n", *argv);
        send_escaped_line(sd, *argv);
        *argv++;
    }
    
    send(sd, "[STDIO]\n", 8, 0);
    
    Debug("waiting for OK message\n");
    if (recv(sd, &buf, 3, 0) != 3) {
        perror("failed to get OK message");
        exit(1);
    }
    if (strncmp(buf, "OK\n", 3)) {
        fprintf(stderr, "incorrect ok message: %s\n", buf);
        exit(1);
    }
    
    /* set flags */
    sock_flags = fcntl(sd, F_GETFL);
    fcntl(sd, F_SETFL, sock_flags | O_NONBLOCK);
    
    /* set flags */
    sock_flags = fcntl(errsd, F_GETFL);
    fcntl(errsd, F_SETFL, sock_flags | O_NONBLOCK);
    
    free(sock_name);
    
    Debug("reading results\n");
    DoIO(sd, errsd);
    
    Debug("exiting\n");
}

void
ProcessError( int errsd )
{
    char buf[1];
    int readlen;
    
    Debug("reading stderr\n");
    while (readlen = read(errsd, &buf, 1)) {
        if (readlen == -1) {
            /* might be EAGAIN (deadlock). Try writing instead */
            if (errno != EAGAIN) {
                perror("read");
            }
            break;
        }
        else {
            // Debug(buf);
            write(2, buf, 1);
        }
    }
}

void
DoIO( int sd, int errsd )
{
    char c;
    fd_set rfds, wfds, stdin_fds;
    struct timeval tv;
    int retval, maybe_read, maybe_write, readlen;
    char buf[1];
    struct sockaddr name;
    socklen_t namelen;
    int sock_flags;
    
    maybe_read = maybe_write = 1;
    
    namelen = sizeof(name);
    
    /* set stdin to non-blocking */
    // sock_flags = fcntl(0, F_GETFL);
    // fcntl(0, F_SETFL, sock_flags | O_NONBLOCK);
    
    signal(SIGINT, sig_handler);
    
    // while we're connected...
    while (!skreech_to_a_halt) {
        int rval, rval_len;
        int data = 0;
        
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        
        FD_ZERO(&stdin_fds);
        FD_SET(0, &stdin_fds);
        
        // for some reason, if I make usec == 0 (poll), performance sucks
        tv.tv_sec = 0;
        tv.tv_usec = 1;
        
        Debug("checking for read...\n");
        
        if (select(sd + 1, &rfds, NULL, NULL, &tv)) {
            /* can read */
            Debug("can read...\n");
            if (FD_ISSET(sd, &rfds)) {
                Debug("reading stdout\n");
                while (readlen = read(sd, &buf, 1)) {
                    if (readlen == -1) {
                        /* might be EAGAIN (deadlock). Try writing instead */
                        if (errno != EAGAIN) {
                            perror("read");
                        }
                        break;
                    }
                    else {
                        // Debug(buf);
                        write(1, buf, 1);
                    }
                }
                if (readlen == 0) {
                    /* all done. remote end closed socket */
                    // perror("foo");
                    break;
                }
            }
        }
        
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        Debug("checking for stdin/write...\n");
        
        if (select(1, &stdin_fds, NULL, NULL, &tv)) {
            Debug("something to send\n");
            /* something on stdin to send */
            tv.tv_sec = 0;
            tv.tv_usec = 1;
            
            FD_ZERO(&wfds);
            FD_SET(sd, &wfds);
            
            while (select(sd + 1, NULL, &wfds, NULL, &tv)) {
                /* and we can send it! */
                Debug("and we can send it\n");
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                readlen = read(0, &c, 1);
                if (readlen < 1) {
                    // ignore STDIN read errors and closed STDIN
                    if (errno != EAGAIN) {
                        perror("stdin");
                    }
                    break;
                }
                send(sd, &c, 1, 0);
                
                // only send a line at a time
                if (c == '\n') {
                    break;
                }
            }
        }
        
        // getpeername can tell us if we're still connected
        if (getpeername(sd, &name, &namelen) != 0) {
            perror("getpeername");
            shutdown(sd, 2);
            close(sd);
            fflush(NULL);
            break;
        }
    }
    
    if (skreech_to_a_halt) {
        // perror("closing socket");
        shutdown(sd, 2);
        close(sd);
        fflush(NULL);
        exit(1);
    }
    
    return;
    
}

