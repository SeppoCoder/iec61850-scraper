#ifndef USEFULMACROS
#define USEFULMACROS
#define LOG(p, ...) (fprintf(p, __VA_ARGS__), printf(__VA_ARGS__))
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif