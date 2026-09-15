#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int ping[2], pong[2];
  int pid, sender, status;

  if (pipe(ping) < 0) {
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }
  if (pipe(pong) < 0) {
    close(ping[0]);
    close(ping[1]);
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    close(ping[0]);
    close(ping[1]);
    close(pong[0]);
    close(pong[1]);
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    close(ping[1]);
    close(pong[0]);
    if (read(ping[0], &sender, sizeof(sender)) != sizeof(sender)) {
      fprintf(2, "pingpong: read ping failed\n");
      exit(1);
    }
    close(ping[0]);
    printf("%d: received ping from pid %d\n", getpid(), sender);

    sender = getpid();
    if (write(pong[1], &sender, sizeof(sender)) != sizeof(sender)) {
      fprintf(2, "pingpong: write pong failed\n");
      exit(1);
    }
    close(pong[1]);
    exit(0);
  }

  close(ping[0]);
  close(pong[1]);
  // Send the sender's PID through each pipe, rather than assuming PID values.
  sender = getpid();
  status = 0;
  if (write(ping[1], &sender, sizeof(sender)) != sizeof(sender)) {
    fprintf(2, "pingpong: write ping failed\n");
    status = 1;
  }
  close(ping[1]);
  if (read(pong[0], &sender, sizeof(sender)) != sizeof(sender)) {
    fprintf(2, "pingpong: read pong failed\n");
    status = 1;
  } else {
    printf("%d: received pong from pid %d\n", getpid(), sender);
  }
  close(pong[0]);
  int child_status;
  if (wait(&child_status) < 0 || child_status != 0) status = 1;
  exit(status);
}
