# Amoeba reference measurements

Everything here was measured against **real Amoeba**, not quoted from the
literature, except where a section explicitly says "published".

## What was run

**FSD-Amoeba 2002A** (`fsd-2002B.iso`, 636 MB, SourceForge project
`fsd-amoeba`) — the i386 packaging of Amoeba 5.3, which is free software
under an XFree86-style licence. Booted under `qemu-system-i386` on the
same host that runs Hydra's `qemu-system-riscv64` benchmarks.

Kernel: `FSD-Amoeba 2002-A Standalone Boot Program, version 2.0 (Dec 31 2001)`
FLIP: `Fast Local Internet Protocol Switch, Version 1.9 (Dec 28 2001)`
Bullet: `File Server version 2.0b (Dec 26 2001)`
Soap: `Directory and Name server version 2.1 (Dec 26 2001)`

Host-side scripts live in `/home/sahan/amoeba-ref/` (outside this repo, as
the ISO and disk images are large):

    boot_to_shell.sh   cold-boot and drive the 3 install floppies to a shell
    amdrive.sh         QEMU-monitor helpers (sendkey/screendump)
    mkvol.py           build Amoeba multi-volume install floppies
    amoeba-hd.img      2 GB disk: Amoeba partition + 4 virtual disks

## Benchmarks used

Amoeba's **own** performance tests, written by the VU authors and shipped
in the distribution — not re-implementations:

    module/amoeba/src/test/performance/syscall/sys_null.c  -> Tperf_null
    module/amoeba/src/test/performance/rpc/rpc_client.c    -> Trpc_client
    module/amoeba/src/test/performance/rpc/rpc_server.c    -> Trpc_server

They ship precompiled for i386 in `basic/test.starchgz` on the CD.

## Measured: null system call

`Tperf_null <count>` — count x `sys_null()`, timed with `time()`.
`time()` has 1-second resolution, so a large count is required; 60,000,000
puts the run at ~45 s and makes the quantisation error ~1%.

| run | calls | seconds | calls/sec | us/call |
|-----|------:|--------:|----------:|--------:|
| 1   | 60,000,000 | 45 | 1,333,333 | 0.750 |
| 2   | 60,000,000 | 47 | 1,276,595 | 0.783 |

**Mean: 0.767 us per null system call.**

## Measured: null RPC (local, user-to-user)

    Trpc_server benchport 8192 0 &
    Trpc_client -m 50000 benchport 0 500000

`-m` reports every 50,000 RPCs using `sys_milli()` (millisecond
resolution), which avoids the 1-second quantisation above. Request and
reply size 0 — this is the null RPC. Client and server are separate
user processes on the same machine, so it is Amoeba's *local* RPC.

    50000 rpc_trans calls took  982 msec   -> 19.64 us
    50000 rpc_trans calls took 1000 msec   -> 20.00 us
    50000 rpc_trans calls took  992 msec   -> 19.84 us
    50000 rpc_trans calls took  989 msec   -> 19.78 us
    50000 rpc_trans calls took 1014 msec   -> 20.28 us
    50000 rpc_trans calls took 1015 msec   -> 20.30 us
    50000 rpc_trans calls took  998 msec   -> 19.96 us
    50000 rpc_trans calls took 1002 msec   -> 20.04 us
    50000 rpc_trans calls took  979 msec   -> 19.58 us

**Mean: 19.94 us, sd 0.25, n = 9.**

An earlier run on a loaded host gave 18-56 us with the same binary; the
figures above are from an otherwise-idle host and are tight (1.3% sd).
The noisy run is discarded as host contention, not Amoeba behaviour.

## The ratio (what Chapter 5 compares)

Absolute microseconds here are **not** comparable to Hydra's: different
ISA, different emulated machine, different QEMU translation cost. The
ratio is, because both paths pay the same per-trap overhead on the same
host.

| system | null syscall | RPC / IPC round trip | ratio |
|--------|-------------:|---------------------:|------:|
| Amoeba (i386, QEMU)      | 0.767 us    | 19.94 us      | **26.0x** |
| Hydra (RISC-V, QEMU)     | 4,804 cyc   | 190,229 cyc   | **39.7x** |

Hydra's IPC round trip costs 39.7 null syscalls; Amoeba's RPC costs 26.0.
Amoeba's RPC is therefore about **1.5x more efficient relative to its own
syscall floor**.

Caveat to state in the write-up: the two are not identical operations.
Amoeba's `rpc_trans` is a single kernel-mediated request/reply between two
user processes. Hydra's round trip is `SYS_SEND_MSG` + `SYS_RECV_MSG` to a
*supervisor-mode* echo server, and includes whatever scheduling latency it
takes for that server to be run. Part of Hydra's 39.7x is scheduler
latency, not IPC cost — which is itself a finding worth reporting.

## Published figures (literature, for historical context)

From Tanenbaum, van Renesse, van Staveren, Sharp, Mullender, Jansen and
van Rossum, "Experiences with the Amoeba Distributed Operating System",
*CACM* 33(12), Dec 1990, pp. 46-63 — Fig. 5 and Fig. 6, p. 13 of the VU
reprint (`cs.vu.nl/~ast/Publications/Papers/cacm-1990.pdf`).

Fig. 5, RPC between user processes, Sun 3/60 over Ethernet:

| case | pure Amoeba remote | Sun RPC remote |
|------|-------------------:|---------------:|
| 4 bytes (delay)  | **1.1 ms**   | 6.7 ms |
| 8 Kb (bandwidth) | **721 KB/s** | 325 KB/s |
| 30 Kb (bandwidth)| **783 KB/s** | n/a |

Local (same processor) 4-byte RPC: 0.5 ms.

Fig. 6, null-RPC latency reported in the literature for other systems:

| system | hardware | null RPC | throughput |
|--------|----------|---------:|-----------:|
| Amoeba   | Sun 3/60 | 1.1 ms | 783 KB/s |
| Cedar    | Dorado   | 1.1 ms | 250 KB/s |
| x-Kernel | Sun 3/75 | 1.7 ms | 860 KB/s |
| V        | Sun 3/75 | 2.5 ms | 546 KB/s |
| Topaz    | Firefly  | 2.7 ms | 587 KB/s |
| Sprite   | Sun 3/75 | 2.8 ms | 720 KB/s |
| **Mach** | Sun 3/60 | **11.0 ms** | not reported |

The Mach figure pairs with Figure 4.7 in the current report
(`docs/report/figures/figure-4-7.png`, relative to the repository root).

## Measured: kernel TCB

From `wc -l` on the real 2001 source tree (`src-kernel-2002a.tgz` on the
CD, 287 .c/.h/.s files), against the current Hydra tree.

| in-kernel component | Amoeba | Hydra |
|---------------------|-------:|------:|
| RPC / IPC           | `ipc/` **3,752** | `ipc.{c,h}` **450** |
| Process mgmt + migration | `server/process/` **3,020** | `migration.{c,h}` **393** |
| Whole kernel, all architectures | **115,824** | **5,558** |

`server/process/` excludes `map.c.orig`, an editor backup left in the
tree; including it would inflate the figure to 3,911.

Amoeba's kernel contains **14 in-kernel servers** totalling 16,723 lines:

| server | lines | | server | lines |
|--------|------:|-|--------|------:|
| process | 3,020 | | bootuser | 627 |
| tty     | 2,927 | | tape     | 424 |
| flip_ip | 2,648 | | kdir     | 350 |
| vdisk   | 1,738 | | printbuf | 166 |
| syssvr  | 1,667 | | tod      | 146 |
| ethraw  | 1,555 | | random   | 113 |
| iop     |   704 | | | |
| profile |   638 | | | |

`server/process/exception.c` implements `PS_CHECKPOINT` and
`control.c` implements immigrating threads, so Amoeba's process
migration is genuinely in-kernel. That is the direct functional
counterpart to Hydra's `migration.{c,h}`, and it is **7.7x** the size.

## Reproducing

    cd /home/sahan/amoeba-ref
    ./boot_to_shell.sh                      # to the amoeba% prompt

    # one-time, already done on amoeba-hd.img:
    #   /super/admin/bin/mkfs -b 12 -f /super/hosts/coldstart/vdisk:04

    /super/admin/bin/bullet -i 2000 -t3 -m0x300000 \
        /super/hosts/coldstart/vdisk:04 &
    mkd /home/t ; cd /home/t
    # insert TEST-0.image, then TEST-1.image when prompted:
    readvol /super/hosts/coldstart/floppy:00 | gzip -d | \
        starch -rv -if - -B /super/cap/ubulletsvr/default

    /home/t/module/test/Tperf_null 60000000
    /home/t/module/test/Trpc_server benchport 8192 0 &
    /home/t/module/test/Trpc_client -m 50000 benchport 0 500000

Do not leave `Trpc_server` running while starting a long `Tperf_null`
sweep: that combination panicked the kernel with
`KERNEL PANIC: interrupt queue overflow`.

## Notes on getting it to run at all

Two things needed fixing before any of this worked, both worth a
sentence in the write-up as emulation-era artefacts:

1. **Amoeba's `fdisk` writes a partition entry but not the 0xAA55 MBR
   boot signature.** On a blank QEMU image with no pre-existing MBR the
   partition is then invisible to Amoeba's own VDISK server, which
   reports `WARNING: invalid partition table (0000)` at boot. A 2002-era
   PC would have had a signature written by DOS `fdisk`. Writing the two
   bytes from the host fixed it immediately.

2. **The installer cannot read the CD.** `IDE1: no devices found` for
   the ATAPI drive on every boot, and the setup script contains no
   reference to a CD-ROM at all. Packages are loaded from floppy volumes
   with `readvol $FLOP | gzip -d | starch -rv -if -`. `mkvol.py`
   reconstructs that floppy format so arbitrary packages can be fed in;
   its output is byte-identical to the shipped `AMOEBA-0.image` when
   given the same payload.
