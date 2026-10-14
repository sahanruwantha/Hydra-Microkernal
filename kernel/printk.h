#ifndef PRINTK_H
#define PRINTK_H

void printk(const char *fmt, ...);

/*
 * Per-packet / per-syscall tracing.
 *
 * These fire on every net syscall, including the "no packet available"
 * case, which is by far the most common outcome. Left enabled they emit
 * ~3,300 lines/s (~250 KB/s) per node, which fills the disk and drowns
 * the useful output. Compiled out by default; build with
 * -DTRACE_PACKETS=1 (or make TRACE_PACKETS=1) to get the full trace back
 * when you actually need to debug the packet path.
 */
#ifndef TRACE_PACKETS
#define TRACE_PACKETS 0
#endif

#if TRACE_PACKETS
#define trace_pk(...) printk(__VA_ARGS__)
#else
#define trace_pk(...) ((void)0)
#endif

/*
 * Scheduler context-switch / yield tracing. Fires on every scheduling
 * decision, so it is the dominant log source once packet tracing is off
 * (~52% of remaining output). Compiled out by default; build with
 * -DTRACE_SCHED=1 to debug scheduling behaviour.
 *
 * Note this does NOT gate the BENCH_* timestamps or the [migration]
 * lines — those are benchmark data and always emitted.
 */
#ifndef TRACE_SCHED
#define TRACE_SCHED 0
#endif

#if TRACE_SCHED
#define trace_sched(...) printk(__VA_ARGS__)
#else
#define trace_sched(...) ((void)0)
#endif

#endif
