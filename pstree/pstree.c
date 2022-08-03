#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
 * -p, --show-pids: 打印每个进程的进程号。
 * -n --numeric-sort: 按照pid的数值从小到大顺序输出一个进程的直接孩子。
 * -V --version: 打印版本信息。
*/
struct {
  int pflag;
  int nflag;
  int vflag;
} clflags = {0, 0, 0};

struct TreeNode {
  int pid, ppid;
  char name[256];
  struct TreeNode *firstchild, *nextsibling;
};

struct vector{
  int current_length;
  int max_size;
  int *array;
};

typedef struct vector vector;
typedef struct TreeNode TreeNode;

void doubleSpace(vector* vec) {
    vec->array = (int *)realloc(vec->array, 2 * vec->max_size * sizeof(int));
    if (vec->array == NULL) {
        printf("doubleSpace failed.\n");
        exit(-1);
    }
    vec->max_size *= 2;
}

void push(vector* vec, int value) {
  if (vec->current_length == vec->max_size)
    doubleSpace(vec);
  vec->array[vec->current_length] = value;
  vec->current_length++;
}

vector* init_vector() {
  vector* vec = malloc(sizeof(vector));
  if (vec == NULL) { 
    printf("vector initialization failed.\n");
    exit(-1);
  }
  vec->current_length = 0;
  vec->max_size = 10;
  vec->array = malloc(vec->max_size * sizeof(int));
  if (vec->array == NULL) {
    printf("vector initialization failed.\n");
    exit(-1);
  }
  return vec;
}

void delete_vector(vector *vec) {
  free(vec->array);
  free(vec);
}

int cmp(const void* a, const void* b) {
    return ( *(int*)a - *(int*)b );
}
/*
 * HashMap is used to store mapping from pid to the pointer,
 * which is useful when building tree;
 */
TreeNode* hashMap[100000];

int openproc();
TreeNode* initialize();
TreeNode* createTreeNode(int pid, int ppid, char *name);
void appendToParent(int ppid, TreeNode* cur);
TreeNode *getNodeInfo(char dname[256]);
void output(TreeNode *root, int flag);
void Traverse(TreeNode *root, int prefix, int flag);
void version();
void numeric_sort(TreeNode *root, int prefix);

int main(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    //printf("argv[%d] = %s\n", i, argv[i]);
    if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
      clflags.vflag = 1;
    } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0) {
      clflags.nflag = 1;
    } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids") == 0) {
      clflags.pflag = 1;
    }
  }
  assert(!argv[argc]);
  if (clflags.vflag) 
    version();
  else if (clflags.pflag) 
    return openproc(1);
  else if (clflags.nflag) 
    return openproc(2);
  else if (argc != 1) {
    printf("Invalid arguments. Please check your input.\n");
    exit(-1);
  } else 
    return openproc(0);
}

TreeNode* initialize() {
  return createTreeNode(0, -1, NULL);
}

int openproc(int flag) {
  DIR *proc = opendir("/proc");
  struct dirent *dir;

  int total = 0;

  TreeNode *root = initialize();

  if (proc) {
    while ((dir = readdir(proc)) != NULL) {
      char *ch = dir->d_name;
      // filter non-numeric dirs
      while (*ch != '\0') {
        if (!isdigit(*ch)) break;
        else ch++;
      }
      if (*ch != '\0') continue;

      char target[256];
      
      //printf("%s\n", dir->d_name);
      
      TreeNode *node = getNodeInfo(dir->d_name);
      appendToParent(node->ppid, node);
      //printf("(%d, %d)\n", pid, ppid);

      // count the number of processes in the system.
      total++;
    }
    closedir(proc);
  } else {
    return -1;
  }
  output(root->firstchild, flag);
  for (int i = 0; i < total; i++) {
    free(hashMap[i]);
  }
  return 0;
}

TreeNode* getNodeInfo(char dname[256]) {
    int pid = atoi(dname);
    char target[256];
    char sppid[256];
    char nname[256];

    sprintf(target, "/proc/%s/stat", dname);

    FILE* fptr = fopen(target, "r");
    fscanf(fptr, "%s ", sppid);
    fscanf(fptr, "(%s)", nname);
    for (int i = 0; i < 2; i++) {
        fscanf(fptr, "%s ", sppid);
    }
    fclose(fptr);

    char *name = malloc((strlen(nname)) * sizeof(char));
    if (name == NULL) {
      printf("Error, malloc failed.\n");
      exit(-1);
    }
    snprintf(name, strlen(nname), "%s", nname);
    //printf("%s\n", name);
    return createTreeNode(pid, atoi(sppid), name);
}

TreeNode* createTreeNode(int pid, int ppid, char *name) {
    TreeNode *node = malloc(sizeof(TreeNode));
    if (node == NULL) {
      printf("Error, malloc failed.\n");
      exit(-1);
    }
    if (name == NULL) {
        node->name[0] = '\0';
    } else {
        strncpy(node->name, name, strlen(name));
        free(name);
    }
    node->firstchild = NULL;
    node->nextsibling = NULL;
    node->ppid = ppid;
    node->pid = pid;
    hashMap[pid] = node;
    return node;
}

void appendToParent(int ppid, TreeNode* cur) {
  assert(ppid >= 0 && cur != NULL);
  if (hashMap[ppid]->firstchild == NULL) {
    hashMap[ppid]->firstchild = cur;
    return;
  }
  TreeNode *sibling = hashMap[ppid]->firstchild;
  while (sibling->nextsibling != NULL) {
    sibling = sibling->nextsibling;
  }
  sibling->nextsibling = cur;
}

/*
 * flag = 0: 无参数
 * flag = 1: -p, --show-pids: 打印每个进程的进程号。
 * flag = 2: -n, --numeric-sort: 按照pid的数值从小到大顺序输出一个进程的直接孩子
 */
void output(TreeNode *root, int flag) {
    printf("(root)-");
    switch(flag) {
      case 0: printf("+--%s\n", root->name); 
              Traverse(root->nextsibling, 0, flag);
              Traverse(root->firstchild, 1, flag);
              break;
      case 1: printf("+--%s(%d)\n", root->name, root->pid);
              Traverse(root->nextsibling, 0, flag);
              Traverse(root->firstchild, 1, flag);
              break;
      case 2: numeric_sort(root, 0); break;
      default: 
              printf("Error argument, please check your arguments passsed to pstree.\n");
              exit(-1);        
    } 
}

void Traverse(TreeNode *root, int prefix, int flag) {
    //printf("%d\n", prefix);
    if (root == NULL) return;
    // char *pre;
    // char *firstline, *nextline;
    printf("       ");
    for (int i = 0; i < prefix; i++)
      printf("|---");
    switch(flag) {
      case 0: printf("+--%s\n", root->name); 
              Traverse(root->nextsibling, prefix, flag);
              Traverse(root->firstchild, prefix + 1, flag);
              break;
      case 1: printf("+--%s(%d)\n", root->name, root->pid);
              Traverse(root->nextsibling, prefix, flag);
              Traverse(root->firstchild, prefix + 1, flag);
              break;
      case 2: numeric_sort(root, 0);
      default: 
              printf("Error argument, please check your arguments passsed to pstree.\n");
              exit(-1);        
    } 
}

void version() {
    puts("pstree 2.0");
    puts("Copyright (C) 2022-2022 clingfei");
    exit(0);
}

void numeric_sort(TreeNode *root, int prefix) {
    //printf("%s %d\n", root->name, root->pid);
  if (root == NULL) return;
  //遍历每个进程的nextsibling，将同一级的进程号放入数组后排序
  //按照hashMap，将进程号作为索引，输出每个进程的名字
  //然后再创建一个新数组，将旧数组中每个进程的nextsibling遍历，并将进程号放入数组后排序，重复此过程。
  vector *vec = init_vector();
    
  while (root != NULL) {
    push(vec, root->pid);
    //printf("%s %d", root->name, root->pid);
    root = root->nextsibling;
  }
    
  qsort(vec->array, vec->current_length, sizeof(int), cmp);
  for (int i = 0; i < vec->current_length; i++) {
    printf("      ");
    for (int j = 0; j < prefix; j++)    printf("|---");
    printf("+--%s\n", hashMap[vec->array[i]]->name);
    numeric_sort(hashMap[vec->array[i]]->firstchild, prefix + 1);
  }
  delete_vector(vec);
}