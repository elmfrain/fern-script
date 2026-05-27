#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#define LogInfo(fmt) fprintf(stdout, "[Info]: " fmt "\n")
#define LogDebug(fmt) fprintf(stdout, "[Debug]: " fmt "\n")
#define LogWarning(fmt) fprintf(stdout, "[Warning]: " fmt "\n")
#define LogError(fmt) fprintf(stderr, "[Error]: " fmt "\n")
#define LogFatal(fmt) fprintf(stderr, "[Fatal]:" fmt "\n")

#define LogInfoV(fmt, ...) fprintf(stdout, "[Info]: " fmt "\n", __VA_ARGS__)
#define LogDebugV(fmt, ...) fprintf(stdout, "[Debug]: " fmt "\n", __VA_ARGS__)
#define LogWarningV(fmt, ...) fprintf(stdout, "[Warning]: " fmt "\n", __VA_ARGS__)
#define LogErrorV(fmt, ...) fprintf(stderr, "[Error]: " fmt "\n", __VA_ARGS__)
#define LogFatalV(fmt, ...) fprintf(stderr, "[Fatal]:" fmt "\n", __VA_ARGS__)

#endif // LOGGING_H
