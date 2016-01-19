/* Code based on Stevens' UNIX Network Programming, and simplified. */
/* Florian Weimer <fw@deneb.enyo.de>, April 2008 */

#include	<errno.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include        <unistd.h>
#include	"pass_fd.h"

int
s_pipe(int fd[2])
{
  return socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
}

int
send_fd(int clifd, int fd)
{
  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_aligned;

  struct msghdr msg = {0};
  msg.msg_control = control_aligned.control;
  msg.msg_controllen = sizeof(control_aligned.control);

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  *((int *) CMSG_DATA(cmptr)) = fd;

  char buf[1] = {0};
  struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  if (sendmsg(clifd, &msg, 0) != 1)
    return -1;
  return 0;
}

int
recv_fd(int servfd)
{
  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_aligned;

  struct msghdr msg = {0};
  msg.msg_control = control_aligned.control;
  msg.msg_controllen = sizeof(control_aligned.control);

  char buf[1] = {0};
  struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  ssize_t result = recvmsg(servfd, &msg, 0);
  if (result < 0)
    return -1;
  if (result != 1) {
    errno = EINVAL;
    return -1;
  }

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  if (cmptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int))
      && cmptr->cmsg_level == SOL_SOCKET && cmptr->cmsg_type == SCM_RIGHTS)
    return *((int *) CMSG_DATA(cmptr));
  errno = ENXIO;
  return -1;
}
