#pragma once

void klog_init(void);
void klog(const char *msg);
void klogf(const char *fmt, ...);
void klog_dump(void);
unsigned klog_lines(void);
