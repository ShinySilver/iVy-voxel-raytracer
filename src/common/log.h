#pragma once
#include <cstdio>
#include <cstdlib>
#include <csignal>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define debug(_message, ...) {printf("[DEBUG] %s:%d ", __FILE__, __LINE__); printf(_message, ##__VA_ARGS__); printf("\n");}
#define info(_message, ...) {printf("[INFO] %s:%d ", __FILE__, __LINE__); printf(_message, ##__VA_ARGS__); printf("\n");}
#define warn(_message, ...) {printf(ANSI_COLOR_YELLOW "[WARN] %s:%d ", __FILE__, __LINE__); printf(_message, ##__VA_ARGS__); printf(ANSI_COLOR_RESET "\n");}
#define error(_message, ...) {fprintf(stderr, ANSI_COLOR_RED "[ERROR] %s:%d ", __FILE__, __LINE__); fprintf(stderr, _message, ##__VA_ARGS__); fprintf(stderr, ANSI_COLOR_RESET "\n");}
#define fatal(_message, ...) {fprintf(stderr, ANSI_COLOR_RED "[FATAL] %s:%d ", __FILE__, __LINE__); fprintf(stderr, _message, ##__VA_ARGS__); fprintf(stderr, ANSI_COLOR_RESET "\n"); raise(SIGSEGV); __builtin_trap();}

#ifdef assert
#undef assert
#endif

#ifndef NDEBUG
#define assert(expr) if(!(expr)) fatal("Assertion \"%s\" failed. Program will exit.", #expr)
#else
#define assert(expr)
#endif