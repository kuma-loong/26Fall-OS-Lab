#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#include "ai_student_impl.h"

extern void **ai_student_prefetch_context(struct ai_session *);

enum { EMPTY, PENDING, READY, FAILED };
struct prefetch_state {
  int pid;
  int command;
  int data;
  int window;
  int request;
  int next;
  char payload[AI_KV_FILE_BYTES];
};

static int
transfer(int fd, void *buffer, int length, int writing)
{
  char *p = buffer;
  while(length > 0){
    int n = writing ? write(fd, p, length) : read(fd, p, length);
    if(n <= 0)
      return -1;
    p += n;
    length -= n;
  }
  return 0;
}

static void
helper(struct ai_session *session, struct prefetch_state *state,
       int command, int data)
{
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  // Never advance the inherited parent's open-file offset.
  if(session->state[1] >= 0)
    close(session->state[1]);
  int fd = open(name, O_RDONLY);
  int expected = 0;
  int status = 0;
  if(fd < 0)
    status = 1;
  while(status == 0){
    int request;
    if(transfer(command, &request, sizeof(request), 0) < 0)
      break; // Closing the command pipe also requests shutdown.
    if(request == -1)
      break;
    if(request != expected || request < 0 || request >= session->requests ||
       transfer(fd, state->payload, AI_KV_FILE_BYTES, 0) < 0 ||
       transfer(data, &request, sizeof(request), 1) < 0 ||
       transfer(data, state->payload, AI_KV_FILE_BYTES, 1) < 0){
      status = 1;
      break;
    }
    expected++;
  }
  if(fd >= 0)
    close(fd);
  close(command);
  close(data);
  exit(status);
}

static struct prefetch_state *
context(struct ai_session *session)
{
  void **slot = ai_student_prefetch_context(session);
  return slot ? *slot : 0;
}

int
ai_student_prefetch_begin(struct ai_session *session)
{
  void **slot = ai_student_prefetch_context(session);
  if(slot == 0 || *slot || session->requests < 1)
    return -1;
  struct prefetch_state *state = malloc(sizeof(*state));
  if(state == 0)
    return -1;
  memset(state, 0, sizeof(*state));
  int command[2], data[2];
  if(pipe(command) < 0){
    free(state);
    return -1;
  }
  if(pipe(data) < 0){
    close(command[0]);
    close(command[1]);
    free(state);
    return -1;
  }
  int pid = fork();
  if(pid < 0){
    close(command[0]);
    close(command[1]);
    close(data[0]);
    close(data[1]);
    free(state);
    return -1;
  }
  if(pid == 0){
    close(command[1]);
    close(data[0]);
    helper(session, state, command[0], data[1]);
  }
  close(command[0]);
  close(data[1]);
  // Only the helper reads the KV stream in this mode.
  if(session->state[1] >= 0){
    close(session->state[1]);
    session->state[1] = -1;
  }
  state->pid = pid;
  state->command = command[1];
  state->data = data[0];
  state->window = EMPTY;
  state->request = -1;
  *slot = state;
  return 0;
}

int
ai_student_prefetch_next(struct ai_session *session, int request)
{
  struct prefetch_state *state = context(session);
  if(state == 0 || state->window != EMPTY || request < 0 ||
     request >= session->requests || request != state->next)
    return -1;
  // One four-byte command, no file reads or response waits in this call.
  if(transfer(state->command, &request, sizeof(request), 1) < 0){
    state->window = FAILED;
    return -1;
  }
  state->request = request;
  state->window = PENDING;
  return 0;
}

int
ai_student_prefetch_wait(struct ai_session *session, int request)
{
  struct prefetch_state *state = context(session);
  if(state == 0 || state->window != PENDING || request != state->request)
    return -1;
  int reply;
  if(transfer(state->data, &reply, sizeof(reply), 0) < 0 ||
     reply != request ||
     transfer(state->data, state->payload, AI_KV_FILE_BYTES, 0) < 0){
    state->window = FAILED;
    return -1;
  }
  state->window = READY;
  return 0;
}

int
ai_student_prefetch_restore(struct ai_session *session, int request,
                            struct kv_entry *kv, struct ai_io *io)
{
  struct prefetch_state *state = context(session);
  if(state == 0 || state->window != READY || request != state->request ||
     kv == 0 || io == 0)
    return -1;
  memmove(kv, state->payload, AI_KV_FILE_BYTES);
  io->kv_read_bytes += AI_KV_FILE_BYTES;
  state->next++;
  state->request = -1;
  state->window = EMPTY;
  return 0;
}

void
ai_student_prefetch_end(struct ai_session *session)
{
  struct prefetch_state *state = context(session);
  if(state == 0)
    return;
  // EOF stops an idle helper. Closing the reply pipe also unblocks a helper
  // still sending a pending record; do this before waiting, never after it.
  close(state->command);
  close(state->data);
  if(state->pid > 0){
    int pid;
    do {
      pid = wait(0);
    } while(pid >= 0 && pid != state->pid);
  }
  *ai_student_prefetch_context(session) = 0;
  free(state);
}
