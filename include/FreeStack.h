#ifndef FREE_STACK_H
#define FREE_STACK_H

#if defined(__AVR__)
#include <Arduino.h>

extern "C" char* __brkval;
extern "C" char __bss_end;

static int FreeStack() {
  char top;
  return __brkval ? &top - __brkval : &top - &__bss_end;
}

static void FillStack() {
  // AVR version of FillStack
}

static int UnusedStack() {
  return FreeStack();
}

#else

static void FillStack() {}
static int UnusedStack() { return 0; }

#endif

#endif
