/* OpenBOR native crash reporting implementation. */
#include "crashhandler.h"

#if defined(CUSTOM_SIGNAL_HANDLER) || defined(__linux__)

#include <signal.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t crash_in_progress = 0;
static char crash_context[128] = "unknown";

void bor_crash_set_context(const char *context)
{
    size_t i;

    if (!context)
        context = "unknown";

    for (i = 0; i < sizeof(crash_context) - 1 && context[i]; ++i)
        crash_context[i] = context[i];

    crash_context[i] = '\0';
}

static const char *signal_name(int signal_number)
{
    switch (signal_number)
    {
        case SIGSEGV: return "SIGSEGV (segmentation fault)";
        case SIGABRT: return "SIGABRT (abort)";
        case SIGBUS:  return "SIGBUS (bus error)";
        case SIGFPE:  return "SIGFPE (arithmetic fault)";
        case SIGILL:  return "SIGILL (illegal instruction)";
        default:      return "UNKNOWN SIGNAL";
    }
}

/* Signal-handler-safe string length. */
static size_t crash_strlen(const char *text)
{
    size_t len = 0;

    if (!text)
        return 0;

    while (text[len])
        ++len;

    return len;
}

static void crash_write(int fd, const char *text)
{
    size_t len;

    if (!text)
        return;

    len = crash_strlen(text);

    while (len > 0)
    {
        ssize_t written = write(fd, text, len);

        if (written <= 0)
            break;

        text += written;
        len -= (size_t)written;
    }
}

static void write_hex_address(int fd, void *address)
{
    static const char hex[] = "0123456789abcdef";
    char buffer[2 + sizeof(unsigned long) * 2 + 1];
    unsigned long value = (unsigned long)address;
    int pos = (int)sizeof(buffer) - 1;

    buffer[pos] = '\0';

    if (!value)
    {
        buffer[--pos] = '0';
    }
    else
    {
        while (value && pos > 2)
        {
            buffer[--pos] = hex[value & 0xf];
            value >>= 4;
        }

        buffer[--pos] = 'x';
        buffer[--pos] = '0';
    }

    crash_write(fd, &buffer[pos]);
}

static void handleFatalSignal(int signal_number, siginfo_t *info, void *context)
{
    int fd;
    void *frames[32];
    int frame_count;

    (void)context;

    if (crash_in_progress)
        _exit(128 + signal_number);

    crash_in_progress = 1;

    fd = open("Logs/openbor_crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd < 0)
        fd = open("openbor_crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd >= 0)
    {
        crash_write(fd, "\n========================================\n");
        crash_write(fd, "OpenBOR native crash report\n");
        crash_write(fd, "========================================\n");
        crash_write(fd, "Signal: ");
        crash_write(fd, signal_name(signal_number));
        crash_write(fd, "\nLua/C context: ");
        crash_write(fd, crash_context);
        crash_write(fd, "\nFault address: ");

        if (info)
            write_hex_address(fd, info->si_addr);
        else
            crash_write(fd, "unknown");

        crash_write(fd, "\n\nBacktrace:\n");

        /* backtrace() is used here only after a fatal signal for diagnostics. */
        frame_count = backtrace(frames, 32);

        if (frame_count > 0)
            backtrace_symbols_fd(frames, frame_count, fd);

        crash_write(fd, "\nEngine terminated by native crash handler.\n");
        close(fd);
    }

    _exit(128 + signal_number);
}

void bor_install_crash_handler(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = handleFatalSignal;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;

    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGBUS,  &action, NULL);
    sigaction(SIGFPE,  &action, NULL);
    sigaction(SIGILL,  &action, NULL);
}

#endif
