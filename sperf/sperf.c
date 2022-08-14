#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

int splitpath(char *pathvar[], char *path);
char* findstrace(char *pathvar[], int pn);

// pipe(pipefd[2])
// pipefd[0]为读端口，pipefd[1]为写端口，父进程和子进程均可以对两者进行读写

int main(int argc, char *argv[]) {
  assert(argc >= 2);

  char *pathvar[100];
  char *path = getenv("PATH");
  int pn = splitpath(pathvar, path);

  int pipefd[2];
  assert(pipe(pipefd) != -1);
  
  if (fork() == 0) {
    // child process
    // close read port.
    close(pipefd[0]);
    char *filename = findstrace(pathvar, pn);
    assert(filename != NULL);
    pathvar[pn] = NULL;

    // construct envp  
    char *envp[2];
    envp[0] = malloc(strlen(filename) + 5);
    sprintf(envp[0], "PATH=%s", path);
    envp[1] = NULL;
    // construct argv
    char *eargv[argc + 2];
    eargv[0] = filename;
    eargv[1] = "-c";
    for (int i = 1; i < argc; i++) {
      eargv[i + 1] = argv[i];
    }
    eargv[argc + 1] = NULL;
    dup2(pipefd[1], 1);
    execve(filename, eargv, envp);
    // garbage collection
    close(pipefd[1]);
    free(envp[0]);
    for (int i = 0; i < pn; i++) {
      free(pathvar[i]);
    }
  } else {
    // parent process
    // close write port.
    close(pipefd[1]);
    char *buffer = malloc(1024);
    FILE* fp = fdopen(pipefd[0], "r");
    size_t n;
    ssize_t flag;
    n = read(pipefd[0], buffer, 1024);
    while ((n = read(pipefd[0], buffer, 1024)) > 0) {
      printf("%s\n", buffer);
    }
    close(pipefd[0]);
    free(buffer);
  }
  // char *exec_argv[] = { "strace", "ls", NULL, };
  // char *exec_envp[] = { "PATH=/bin", NULL, };
  // execve("strace",          exec_argv, exec_envp);
  // execve("/bin/strace",     exec_argv, exec_envp);
  // execve("/usr/bin/strace", exec_argv, exec_envp);
  // perror(argv[0]);
  // exit(EXIT_FAILURE);
}

int splitpath(char *pathvar[], char *path) {
  int i = 0, j = 0;
  int p = 0;
  while (path[j] != '\0') {
    if (path[j] == ':') {
      pathvar[p] = malloc((j - i + 1) * sizeof(char));
      strncpy(pathvar[p], path + i, j - i);
      strcat(pathvar[p], "\0");
      i = ++j;
      ++p;
    } else {
      ++j;
    }
  } 
  pathvar[p] = malloc((j - i + 1) * sizeof(char));
  strncpy(pathvar[p], path + i, j - i);
  strcat(pathvar[p], "\0");
  return ++p;
} 

char *findstrace(char *pathvar[], int pn) {
  struct dirent* d_file;
  for (int i = 1; i < pn; i++) {
    DIR *dir = opendir(pathvar[i]);
    while ((d_file = readdir(dir)) != NULL) {
      if (strcmp(d_file->d_name, "strace") == 0) {
        char *res = pathvar[i];
        strcat(res, "/strace");
        return res;
      }
    }
  }
  return NULL;
}