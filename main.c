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

#include "pass_fd.h" /* the stuff borrowed from stevens */

#define DEBUG 0

/* must never be less than 3 */
#define BUF_SIZE 4096


#ifdef ENOBUFS
#   define NO_BUFSPC(e) ((e) == ENOBUFS || (e) == ENOMEM)
#else
#   define NO_BUFSPC(e) ((e) == ENOMEM)
#endif

static void Usage( char *pName );
static void DecodeParm( char *pArg );
static int  DispatchCall( char *scriptname, int argc, char **argv );

char *pVersion = PPERL_VERSION;
char perl_options[1024];
extern char **environ;
pid_t connected_to;
int kill_script = 0;
int any_user = 0;
int prefork = 5;
int maxclients = 100;
int path_max;

#if DEBUG
#define Dx(x) (x)
#else
#define Dx(x)
#endif

static
void Debug( const char * format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}


int main( int argc, char **argv )
{
    int i;
    char *pArg;
    int pperl_section = 0;
    int return_code = 0;

    if( argc < 2 )
        Usage( argv[0] );

#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf (path, _PC_PATH_MAX);
    if (path_max <= 0) {
        path_max = 4096;
    }
#endif

    pperl_section = 0;
    for ( i = 1; i < argc; i++ ) {
        pArg = argv[i];
        if (*pArg != '-') break;
        if (!strcmp(pArg, "--")) {
            pperl_section = 1;
            continue;
        }
        if (!pperl_section) {
            DecodeParm( pArg );
            continue;
        }
        if ( !strcmp(pArg, "-k") || !strcmp(pArg, "--kill") )
            kill_script = 1;
        else if (!strncmp(pArg, "--prefork", 9) ) {
            int newval;
            if (pArg[9] == '=') /* "--prefork=20" */
                pArg += 10;
            else                /* "--prefork" "20" */
                pArg = argv[++i];

            newval = atoi(pArg);
            if (newval > 0) prefork = newval;
        }
        else if (!strncmp(pArg, "--maxclients", 12) ) {
            int newval;
            if (pArg[12] == '=') /* "--maxclients=20" */
                pArg += 13;
            else                /* "--maxclients" "20" */
                pArg = argv[++i];

            newval = atoi(pArg);
            if (newval > 0) maxclients = newval;
        }
        else if ( !strcmp(pArg, "-z") || !strcmp(pArg, "--anyuser") )
            any_user = 1;
        else if ( !strcmp(pArg, "-h") || !strcmp(pArg, "--help") )
            Usage( NULL );
        else {
            printf("pperl: unknown parameter '%s'\n", pArg);
            Usage( NULL );
        }
    }
    
    i++;
    return_code = DispatchCall( pArg, argc - i, (char**)(argv + i) );
    Dx(Debug("done, returning %d\n", return_code));
    return return_code;
}

static void DecodeParm( char *pArg )
{
    if ( (strlen(perl_options) + strlen(pArg) + 1) > 1000 ) {
        fprintf(stderr, "param list too long. Sorry.");
        exit(1);
    }
    else if ( (strncmp(pArg, "-h", 2) == 0) || (strncmp(pArg, "--help", 6) == 0) ) {
        Usage( NULL );
    }
    strcat(perl_options, pArg);
    strcat(perl_options, " ");
}

static void Usage( char *pName )
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
    printf("perl options are passed to your perl executable (see perl -h).\n"
           "pperl options control the persistent perl behaviour\n"
           "\n"
           "PPerl Options:\n"
           "  -k  or --kill      Kill the currently running pperl for that script\n"
           "  -h  or --help      This page\n"
	   "  --prefork          The number of child processes to prefork (default=5)\n"
	   "  --maxclients       The number of client connections each child\n"
	   "                       will process (default=100)\n"
           "  -z  or --anyuser   Allow any user (after the first) to access the socket\n"
           "                       WARNING: This has severe security implications. Use\n"
	   "                       at your own risk\n"
    );
    exit( 1 );
}

static void *
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
static char *
MakeSockName(char * scriptname )
{
    char * sockname;
    char * save;
    /* strict C compilers can't/won't do char foo[variant]; */
    char *fullpath = my_malloc(path_max);
    int i = 0;

    if (realpath(scriptname, fullpath) == NULL) {
        perror("pperl: resolving full pathname to script failed");
        exit(1);
    }
    Dx(Debug("realpath returned: %s\n", fullpath));
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
    free(fullpath);
    return save;
}


static void
sig_handler(int sig)
{
    kill(connected_to, sig);
    signal(sig, sig_handler);
    /* skreech_to_a_halt++; */
}

static int handle_socket(int sd, int argc, char **argv );
static int DispatchCall( char *scriptname, int argc, char **argv )
{
    register int i, sd, len;
    ssize_t readlen;
    struct sockaddr_un saun;
    char *sock_name;
    char buf[BUF_SIZE];
	sd = 0;

    /* create socket name */
    Dx(Debug("pperl: %s\n", scriptname));
    sock_name = MakeSockName(scriptname);
    Dx(Debug("got socket: %s\n", sock_name));
    
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
        
        Dx(Debug("opening pid_file: %s\n", pid_file));
        pid_fd = open(pid_file, O_RDONLY);
        if (pid_fd == -1) {
            Dx(Debug("Cannot open pid file (perhaps PPerl wasn't running for that script?)\n"));
            write(1, "No process killed - no pid file\n", 32);
            return 0;
        }
        
        readlen = read(pid_fd, buf, BUF_SIZE);
        if (readlen == -1) {
            perror("pperl: nothing in pid file?");
            return 0;
        }
        buf[readlen] = '\0';
        
        close(pid_fd);
        
        pid = atoi(buf);
        Dx(Debug("got pid %d (%s)\n", pid, buf));
        if (kill(pid, SIGINT) == -1) {
            perror("pperl: could not kill process");
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
            perror("pperl: Couldn't create socket");
            return 1;
        }
    }

    saun.sun_family = PF_UNIX;
    strcpy(saun.sun_path, sock_name);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path) + 1;

    Dx(Debug("%d connecting\n", getpid()));

    if (connect(sd, (struct sockaddr *)&saun, len) < 0) {
        /* Consider spawning Perl here and try again */
        FILE *source;
        int tmp_fd;
        char temp_file[BUF_SIZE];
        int start_checked = 0;
        int wrote_footer = 0; /* we may encounter __END__ or __DATA__ */
        int line;

        int pid, itmp, exitstatus;
        sigset_t mask, omask;

        Dx(Debug("Couldn't connect, spawning new server: %s\n", strerror(errno)));

        if (unlink(sock_name) != 0 && errno != ENOENT) {
            perror("pperl: removal of old socket failed");
            exit(1);
        }
        
        /* Create temp file with adjusted script... */
        if (!(source = fopen(scriptname, "r"))) {
            perror("pperl: Cannot open perl script");
            exit(1);
        }

        snprintf(temp_file, BUF_SIZE, "%s/%s", P_tmpdir, "pperlXXXXXX");
        tmp_fd = mkstemp(temp_file);
        if (tmp_fd == -1) {
            perror("pperl: Cannot create temporary file");
            exit(1);
        }
            
        write(tmp_fd, "### Temp File ###\n", 18);
        write(tmp_fd, perl_header, strlen(perl_header));

        line = 0;
        while ( fgets( buf, BUF_SIZE, source ) ) {
            readlen = strlen(buf);
            Dx(Debug("read '%s' %d \n", buf, readlen));

            if (!start_checked) { /* first line */
                start_checked = 1;

                if (buf[0] == '#' && buf[1] == '!') { 
                    char *args;
                    /* solaris sometimes doesn't propogate all the
                     * shebang line  - so we do that here */
                    if ( (args = strstr(buf, " ")) ) {
                        strncat(perl_options, args, strlen(args) - 1);
                    }

                    write(tmp_fd, "\n#line 2 ", 9);
                    write(tmp_fd, scriptname, strlen(scriptname));
                    write(tmp_fd, "\n", 1);

                    line = 2;
                    continue;
                }
                else {
                    write(tmp_fd, "\n#line 1 ", 9);
                    write(tmp_fd, scriptname, strlen(scriptname));
                    write(tmp_fd, "\n", 1);
                }
            }
            if ((!strcmp(buf, "__END__\n") || 
                 !strcmp(buf, "__DATA__\n")) &&
                !wrote_footer) {
                char text_line[BUF_SIZE];
                wrote_footer = 1;
                write(tmp_fd, perl_footer, strlen(perl_footer));
                snprintf(text_line, BUF_SIZE, "package main;\n#line %d %s\n", line, scriptname);
                write(tmp_fd, text_line, strlen(text_line));
            }
            write(tmp_fd, buf, readlen);
            if (buf[readlen] == '\n') ++line;
        }
        
        if (fclose(source)) { 
            perror("pperl: Error reading perl script");
            exit(1);
        }

        if (!wrote_footer) 
            write(tmp_fd, perl_footer, strlen(perl_footer));

        Dx(Debug("wrote file %s\n", temp_file));

        close(tmp_fd);

        /*** Temp file creation done ***/

        snprintf(buf, BUF_SIZE, "%s %s %s %s %d %d %d %s", 
                 PERL_INTERP, perl_options, temp_file,
                 sock_name, prefork, maxclients, 
                 any_user, scriptname);
        Dx(Debug("syscall: %s\n", buf));

        /* block SIGCHLD so noone else can wait() on the child before we do */
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &omask);

        if ((pid = system(buf)) != 0) {
            unlink(temp_file);
            perror("pperl: perl script failed to start");
            exit(1);
        }
        Dx(Debug("waiting for perl to return...\n"));
        while ((itmp = waitpid(0, &exitstatus, 0)) == -1 && errno == EINTR)
            ;
        sigprocmask(SIG_SETMASK, &omask, NULL);
        Dx(Debug("returned.\n"));

        /* now remove the perl script */
        unlink(temp_file);

        /* try and connect to the new socket */
        while ((i++ <= 30) && (connect(sd, (struct sockaddr *)&saun, len) < 0))
        {
            Dx(Debug("."));
            sleep(1);
        }
        if (i >= 30) {
            perror("pperl: persistent perl process failed to start after 30 seconds");
            exit(1);
        }
        Dx(Debug("\n"));
    }
    
    return handle_socket(sd, argc, argv);
}

static 
int 
handle_socket(int sd, int argc, char **argv) {
    long max_fd;
    char **env;
    int i;
    char buf[BUF_SIZE];

    Dx(Debug("connected over %d\n", sd));

    read(sd, buf, 10);
    buf[10] = '\0';
    connected_to = atoi(buf);
    Dx(Debug("chatting to %d, hooking signals\n", connected_to));

    /* bad magic number, there only seem to be 30 signals on a linux
     * box -- richardc*/
    for (i = 1; i < 32; i++) 
        signal(i, sig_handler);


    Dx(Debug("sending fds\n"));
    if ((max_fd = sysconf(_SC_OPEN_MAX)) < 0) {
        perror("pperl: dunno how many fds to check");
        exit(1);
    }

    for (i = 0; i < max_fd; i++) {
        if (fcntl(i, F_GETFL, -1) >= 0 && i != sd) {
            int ret;
            write(sd, &i, sizeof(int));
            ret = send_fd(sd, i);
            Dx(Debug("send_fd %d %d\n", i, ret));
        }
    }
    i = -1;
    write(sd, &i, sizeof(int));
    Dx(Debug("fds sent\n"));

    write(sd, "[PID]", 6);
    snprintf(buf, BUF_SIZE, "%d", getpid());
    write(sd, buf, strlen(buf) + 1);

    
    /* print to socket... */
    write(sd, "[ENV]", 6);
    for (i= 0, env = environ; *env; i++, env++); 
    snprintf(buf, BUF_SIZE, "%d", i);
    write(sd, buf, strlen(buf) + 1);
    
    while ( *environ != NULL ) {
        size_t len = strlen(*environ) + 1;
        /* Dx(Debug("sending environ: %s\n", *environ)); */
        write(sd, *environ, len);
        environ++;
    }

    write(sd, "[CWD]", 6);
    if (getcwd(buf, BUF_SIZE) == NULL) {
        perror("pperl: getcwd");
        exit (1);
    }
    write(sd, buf, strlen(buf) + 1);

    Dx(Debug("sending %d args\n", argc));
    write(sd, "[ARGV]", 7);
    snprintf(buf, BUF_SIZE, "%d", argc);
    write(sd, buf, strlen(buf) + 1);
    for (i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        Dx(Debug("sending argv[%d]: '%s'\n", i, argv[i]));
        write(sd, argv[i], len);
    }

    write(sd, "[DONE]", 7);

    Dx(Debug("waiting for OK message from %d\n", sd));
    if (read(sd, buf, 3) != 3) {
        perror("pperl: failed to read 3 bytes for an OK message");
        exit(1);
    }
    if (strncmp(buf, "OK\n", 3)) {
        i = read(sd, buf, BUF_SIZE - 1);
        buf[i] = '\0';
        fprintf(stderr, "pperl: expected 'OK\\n', got: '%s'\n", buf);
        exit(1);
    }
    Dx(Debug("got it\n"));

    Dx(Debug("reading return code\n"));
    i = read(sd, buf, BUF_SIZE - 1);
    buf[i] = '\0';
    Dx(Debug("socket read '%s'\n", buf));

    for (i = 0; i < max_fd; i++) {
        close(i);
    }
    
    exit (atoi(buf));
}



/* 
Local Variables:
mode: C
c-basic-offset: 4
tab-width: 4
indent-tabs-mode: nil
End:
*/
