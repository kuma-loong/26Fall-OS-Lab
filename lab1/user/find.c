#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Share one path buffer so recursion does not put large arrays on the user stack.
static char path[MAXPATH];

static int find(char *name) {
  int fd, n, result = 0;
  struct stat st;
  struct dirent de;
  char *base = path;
  int len = strlen(path);

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return 1;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return 1;
  }

  for (char *p = path; *p; p++)
    if (*p == '/') base = p + 1;
  if (strcmp(base, name) == 0) printf("%s\n", path);

  if (st.type == T_DIR) {
    int end = len;
    if (end == 0 || path[end - 1] != '/') end++;
    while ((n = read(fd, &de, sizeof(de))) == sizeof(de)) {
      if (de.inum == 0) continue;
      char entry[DIRSIZ + 1];
      memmove(entry, de.name, DIRSIZ);
      entry[DIRSIZ] = 0;
      if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
      if (end + strlen(entry) + 1 > sizeof(path)) {
        fprintf(2, "find: path too long: %s/%s\n", path, entry);
        result = 1;
        continue;
      }
      if (end > len) path[len] = '/';
      strcpy(path + end, entry);
      if (find(name) != 0) result = 1;
      path[len] = 0;
    }
    if (n != 0) {
      fprintf(2, "find: cannot read %s\n", path);
      result = 1;
    }
  }
  close(fd);
  return result;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "usage: find <path> <name>\n");
    exit(1);
  }
  int len = strlen(argv[1]);
  if (len >= sizeof(path)) {
    fprintf(2, "find: path too long\n");
    exit(1);
  }
  strcpy(path, argv[1]);
  while (len > 1 && path[len - 1] == '/') path[--len] = 0;
  exit(find(argv[2]));
}
