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

#define PREFORK 5
#define MAX_CLIENTS_PER_CHILD 100

#define DEBUG 0

/* must never be less than 3 */
#define BUF_SIZE 4096


#ifdef ENOBUFS
#   define NO_BUFSPC(e) ((e) == ENOBUFS || (e) == ENOMEM)
#else
#   define NO_BUFSPC(e) ((e) == ENOMEM)
#endif

void Usage( char *pName );
void DecodeParm( char *pArg );
void DecodePPerlParm( char *pArg );
int  DispatchCall( char *scriptname, int argc, char **argv );

char *pVersion = PPERL_VERSION;
char perl_options[1024];
extern char **environ;
int skreech_to_a_halt = 0;
int kill_script = 0;
int any_user = 0;
int path_max;

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

#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf (path, _PC_PATH_MAX);
    if (path_max <= 0) {
        path_max = 4096;
    }
#endif

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
    else if ( (strncmp(pArg, "-z", 2) == 0) || (strncmp(pArg, "--anyuser", 9) == 0) ) {
        any_user = 1;
    }
    else if ( (strncmp(pArg, "-h", 2) == 0) || (strncmp(pArg, "--help", 6) == 0) ) {
        Usage( NULL );
    }
}

void DecodeParm( char *pArg )
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
    printf("perl options are passed to your perl executable (see perl -h).\n"
           "pperl options control the persistent perl behaviour\n"
           "\n"
           "PPerl Options:\n"
           "  -k  or --kill      Kill the currently running pperl for that script\n"
           "  -h  or --help      This page\n"
           "  -z  or --anyuser   Allow any user (after the first) to access the socket\n"
           "                     WARNING: This has severe security implications if you\n"
           "                     don't know what you are doing. Use at your own risk\n"
    );
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
    char fullpath[BUF_SIZE];
    int i = 0;

    if (realpath(scriptname, fullpath) == NULL) {
        perror("resolving full pathname to script failed");
        exit(1);
    }
    Debug("realpath returned: %s\n", fullpath);
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

static int handle_socket(int sd, int argc, char **argv );
int DispatchCall( char *scriptname, int argc, char **argv )
{
    register int i, sd, len;
    ssize_t readlen;
    struct sockaddr_un saun;
    char *sock_name;
    char buf[BUF_SIZE];
	sd = 0;

    /* create socket name */
    Debug("pperl: %s\n", scriptname);
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

    Debug("%d connecting\n", getpid());

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

        snprintf(syscall, BUF_SIZE, "%s %s %s %s %d %d %d", PERL_INTERP, perl_options, temp_file,
                sock_name, PREFORK, MAX_CLIENTS_PER_CHILD, any_user);
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
    
    return handle_socket(sd, argc, argv);
}

static 
int 
handle_socket(int sd, int argc, char **argv) {
    long max_fd;
    char **env;
    int i;
    char buf[BUF_SIZE];

    Debug("connected over %d\n", sd);

    Debug("sending fds\n");
    if ((max_fd = sysconf(_SC_OPEN_MAX)) < 0) {
        perror("dunno how many fds to check");
        exit(1);
    }

    for (i = 0; i < max_fd; i++) {
        if (fcntl(i, F_GETFL, -1) >= 0 && i != sd) {
            int ret;
            write(sd, &i, sizeof(int));
            ret = send_fd(sd, i);
            Debug("send_fd %d %d\n", i, ret);
        }
    }
    i = -1;
    write(sd, &i, sizeof(int));
    Debug("fds sent\n");

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
        /* Debug("sending environ: %s\n", *environ); */
        write(sd, *environ, len);
        environ++;
    }

    write(sd, "[CWD]", 6);
    if (getcwd(buf, BUF_SIZE) == NULL) {
        perror("getcwd");
        exit (1);
    }
    write(sd, buf, strlen(buf) + 1);

    Debug("sending %d args\n", argc);
    write(sd, "[ARGV]", 7);
    snprintf(buf, BUF_SIZE, "%d", argc);
    write(sd, buf, strlen(buf) + 1);
    for (i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        Debug("sending argv[%d]: '%s'\n", i, argv[i]);
        write(sd, argv[i], len);
    }

    write(sd, "[DONE]", 7);

    Debug("waiting for OK message\n");
    if (read(sd, buf, 3) != 3) {
        perror("failed to get OK message");
        exit(1);
    }
    if (strncmp(buf, "OK\n", 3)) {
        buf[3] = 0;
        fprintf(stderr, "incorrect ok message: %s\n", buf);
        exit(1);
    }
    Debug("got it\n");


    Debug("reading return code\n");
    i = read(sd, buf, BUF_SIZE);
    buf[i] = '\0';
    Debug("socket read '%s'\n", buf);

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
