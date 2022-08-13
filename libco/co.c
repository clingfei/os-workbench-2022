#include "co.h"
#include <stdlib.h>

#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define STACK_SIZE 1 << 17
#include <stdint.h>
#include <time.h>
jmp_buf base;
int lpr_status;
int lpr_selected;
int lpr_now;
int lpr_i;
enum co_status {
  CO_NOTHING, // as its name, it is nothing
  CO_NEW, // 新创建，还未执行过
  CO_RUNNING, // 已经执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
  char name[32];
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;  // 协程的状态
  struct co *    waiter;  // 当前协程是否在等待其他携程，如果在等待，等待的是谁
  jmp_buf        context; // 寄存器现场 (setjmp.h)
  long double buffer;
  uint8_t        stack[STACK_SIZE]; // 协程的堆栈
};

struct co* coPool;
struct co* current = NULL;
int coroutinesCanBeUsed;

void co_init(void) {
  coPool = malloc(192*sizeof(struct co));
  coroutinesCanBeUsed = 0;
  for (int i = 0; i < 192; i++) {
      coPool[i].arg = NULL;
      coPool[i].buffer = 0;
      coPool[i].func = NULL;
      memset(coPool[i].stack, 0, STACK_SIZE);
      coPool[i].status = CO_NOTHING;
      coPool[i].waiter = NULL;
  }
}
struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  assert(coroutinesCanBeUsed < 130);
  coroutinesCanBeUsed += 1;
  int i;
  for (i = 0; i < 192; i++) {
    if (coPool[i].status == CO_NOTHING) {
      coPool[i].arg = arg;
      coPool[i].func = func;
      strcpy(coPool[i].name, name);
      coPool[i].status = CO_NEW;
      break;
    }
  }
  return &coPool[i];
}


static void stack_switch_call(void *sp, void *entry, uintptr_t arg) {

  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
      : : "b"((uintptr_t)sp - 8),     "d"(entry), "a"(arg)
#else
    "movl %0, %%esp; movl %2, 4(%0); jmp *%1"
      : : "b"((uintptr_t)sp - 16), "d"(entry), "a"(arg)
#endif
  );
}


struct co* getNext() {
  lpr_selected = rand() % coroutinesCanBeUsed;
  lpr_now = -1;
  for (lpr_i = 0; lpr_i < 192; lpr_i++) {
    if (coPool[lpr_i].status == CO_RUNNING || coPool[lpr_i].status == CO_WAITING || coPool[lpr_i].status == CO_NEW) {
      lpr_now += 1;
      if (lpr_now == lpr_selected) {
        break;
      }
    }
  }
  struct co* returned = &coPool[lpr_i];
  while (returned->status == CO_WAITING) {
      assert(returned->status != CO_DEAD);
      if (returned->waiter != NULL && returned->waiter->status != CO_DEAD)
        returned = returned->waiter;
      else
        break;
    }
  return returned;

}

static void* co_wrapper(struct co* co) {
  co->status = CO_RUNNING;
  co->func(co->arg);
  co->status = CO_DEAD;
  coroutinesCanBeUsed -= 1;
  co_yield();
  return 0;
}

void co_wait(struct co *co) {
  current->waiter = co;
  current->status = CO_WAITING;
  co_yield();
  current->waiter->status = CO_NOTHING;
  current->waiter = NULL;
  current->status = CO_RUNNING;
}

void co_yield() {
  int val = setjmp(current->context);
  if (val == 0) {
    current = getNext();
    if (current->status == CO_NEW) {
      current->status = CO_RUNNING;
      stack_switch_call(&current->stack[STACK_SIZE], co_wrapper, (uintptr_t) current);
    } else {
      longjmp(current->context, 1);
    }
  }
}

void __attribute__((constructor)) start() {
  srand(time(0));
  co_init();
  coroutinesCanBeUsed += 1;
  strcpy(coPool[0].name, "main");
  current = &coPool[0];
  current->status = CO_RUNNING;
}