#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#include "ai_student_impl.h"

// state[0] is the path flag; KV owns [1..4]. No retained KV payload.
enum { FD = 1, PHASE, WRITTEN, RESTORED };
enum { CLOSED, INITIALIZED, WRITING, STORED, READING, BROKEN };

int
ai_student_kv_begin(struct ai_session *session)
{
  if(session->state[PHASE] != CLOSED)
    return -1;
  session->state[FD] = -1;
  session->state[PHASE] = CLOSED;
  if(session->worker < 0 || session->worker >= AI_MAX_WORKERS ||
     session->requests < 1 || session->requests > AI_MAX_REQUESTS)
    return -1;
  session->state[WRITTEN] = session->state[RESTORED] = 0;
  session->state[PHASE] = INITIALIZED;
  return 0;
}

int
ai_student_kv_begin_write(struct ai_session *session)
{
  if(session->state[PHASE] != INITIALIZED)
    return -1;
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  int fd = open(name, O_CREATE | O_WRONLY | O_TRUNC);
  if(fd < 0)
    return -1;
  session->state[FD] = fd;
  session->state[PHASE] = WRITING;
  return 0;
}

int
ai_student_kv_store(struct ai_session *session, int request,
                    struct kv_entry *kv, struct ai_io *io)
{
  // Reject before touching either the file offset or the caller's buffers.
  if(session->state[PHASE] != WRITING || request < 0 ||
     request >= session->requests || request != session->state[WRITTEN] ||
     kv == 0 || io == 0)
    return -1;
  int offset = 0;
  while(offset < AI_KV_FILE_BYTES){
    int n = write(session->state[FD], (char*)kv + offset,
                  AI_KV_FILE_BYTES - offset);
    if(n <= 0){
      session->state[PHASE] = BROKEN;
      return -1;
    }
    offset += n;
  }
  session->state[WRITTEN]++;
  io->kv_write_bytes += AI_KV_FILE_BYTES;
  return 0;
}

int
ai_student_kv_end_write(struct ai_session *session)
{
  if(session->state[PHASE] != WRITING ||
     session->state[WRITTEN] != session->requests)
    return -1;
  int result = close(session->state[FD]);
  session->state[FD] = -1;
  session->state[PHASE] = result < 0 ? BROKEN : STORED;
  return result;
}

int
ai_student_kv_begin_read(struct ai_session *session)
{
  if(session->state[PHASE] != STORED)
    return -1;
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  int fd = open(name, O_RDONLY);
  if(fd < 0)
    return -1;
  session->state[FD] = fd;
  session->state[RESTORED] = 0;
  session->state[PHASE] = READING;
  return 0;
}

int
ai_student_kv_restore(struct ai_session *session, int request,
                      struct kv_entry *kv, struct ai_io *io)
{
  if(session->state[PHASE] != READING || request < 0 ||
     request >= session->requests || request != session->state[RESTORED] ||
     kv == 0 || io == 0)
    return -1;
  int offset = 0;
  while(offset < AI_KV_FILE_BYTES){
    int n = read(session->state[FD], (char*)kv + offset,
                 AI_KV_FILE_BYTES - offset);
    if(n <= 0){
      session->state[PHASE] = BROKEN;
      return -1;
    }
    offset += n;
  }
  session->state[RESTORED]++;
  io->kv_read_bytes += AI_KV_FILE_BYTES;
  return 0;
}

void
ai_student_kv_end(struct ai_session *session)
{
  if(session->state[PHASE] == CLOSED)
    return;
  if(session->state[FD] >= 0)
    close(session->state[FD]);
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  unlink(name);
  session->state[FD] = -1;
  session->state[PHASE] = CLOSED;
}
