#include "ticker.h"

#include "klog.h"
#include "pipe.h"
#include "pit.h"
#include "printf.h"
#include "sched.h"
#include "string.h"

void ticker_task(void *arg)
{
    (void)arg;
    klog("ticker started");
    for (;;) {
        sched_sleep(2000);
        char msg[80];
        ksnprintf(msg, sizeof(msg), "tick uptime %lu ms",
                  (unsigned long)pit_uptime_ms());
        klog(msg);
        size_t n = strlen(msg);
        if (n + 1 < sizeof(msg)) {
            msg[n++] = '\n';
            msg[n] = '\0';
        }
        pipe_write(PIPE_SYS, msg, n);
    }
}
