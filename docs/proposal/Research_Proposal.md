# Research Proposal — Content for Word Template

**Title:**
User-Space Process Migration on a RISC-V Microkernel: An Empirical Re-Evaluation of the 1999 Adoption Barriers

---

## Chapter 1: Introduction

This project addresses one architectural question: **on a modern RISC-V microkernel, should process migration mechanism live inside the kernel or in user space?** In 1993, Milojičić et al. built both variants on the same Mach microkernel and reported that the kernel-resident version was preferred because user-space inter-process communication (IPC) was too slow to make user-space migration practical (Milojičić et al., 1993; Milojičić et al., 1999, p. 29). That architectural verdict has not been re-examined in three decades, even though three changes since 1999 bear directly on it. First, microkernel IPC performance is now within 5% of monolithic kernels for IPC-intensive workloads (Gu et al., 2020; Chen et al., 2024) — the original reason migration was kept in the kernel no longer holds. Second, process migration is now deployed at hyperscale, no longer a research curiosity: container live migration via CRIU is in production under Kubernetes and Docker (Berrange, 2014; Reber, 2023), QNX runs in 235 million vehicles (BlackBerry, 2024), and software-defined vehicle architectures explicitly require workload migration between zone controllers (Avassa, 2026; Roland Berger, 2024). Third, RISC-V has emerged as the open instruction-set architecture on which contemporary microkernel research is built (seL4 Foundation, 2025; Hensoldt Cyber, 2023; RISC-V International, 2025), and no published reference implementation of process migration exists for any open-source RISC-V microkernel.

This project re-asks the 1993 question on RISC-V. It implements process migration two ways on the same minimal microkernel — once as a kernel-resident mechanism (the Mach 1993 design) and once as a user-space service (the modern microkernel discipline of Liedtke, 1995 and Heiser et al., 2006) — measures the cost in freeze time, total latency, system call count, and kernel trusted-computing-base size, and reports whether the 1990s architectural verdict still holds on contemporary open hardware. The contribution is an empirical comparison and an open-source artefact, not a new algorithm.

### 1.1 Background

Microkernel operating systems minimise the code running in privileged mode and place services such as device drivers, file systems, and networking in user-space processes that communicate via inter-process messages (Liedtke, 1995; Heiser et al., 2006). This design provides strong fault isolation, smaller trusted computing bases, and the ability to evolve services without modifying the kernel (Tanenbaum and Bos, 2014). The seL4 microkernel (Klein et al., 2009) demonstrated that such a kernel can be formally verified for functional correctness and information-flow security (Murray et al., 2013), and microkernels are now the dominant architecture in safety-critical embedded systems including automotive, avionics, and medical devices (BlackBerry, 2024; seL4 Foundation, 2025). Empirical analysis of Linux CVEs has shown that approximately 40% of critical kernel vulnerabilities would be eliminated by a formally verified microkernel architecture, and only 4% would remain unmitigated (Biggs, Lee and Heiser, 2018). RISC-V, an open instruction-set architecture (Waterman and Asanović, 2019), has become the preferred target for new microkernel research because it is royalty-free, supports custom security extensions such as Physical Memory Protection and CHERI-style capabilities (Watson et al., 2020), and is being adopted by both edge AI and SDV industry initiatives (RISC-V International, 2025; Antmicro, 2020).

Process migration on operating systems has a long lineage. Mach (Accetta et al., 1986) implemented three migration variants — a simplified user-level server, an optimised user-level server, and a kernel-resident implementation — and reported performance on 33 MHz 80486 hardware (Milojičić et al., 1993; Milojičić et al., 1999). MOSIX (Barak and Litman, 1985), Sprite (Douglis and Ousterhout, 1991), V (Cheriton, 1988), Accent (Rashid and Robertson, 1981), and Amoeba (Mullender et al., 1990) each implemented migration with different design trade-offs. None of these systems achieved widespread commercial deployment, for the reasons enumerated in Milojičić et al. (1999, §7). Today, however, the dominant deployed process-migration mechanism — Checkpoint-Restore in Userspace (CRIU) underneath Kubernetes — is a Linux-only, monolithic-kernel design (Berrange, 2014; Reber, 2023). No equivalent reference implementation exists for a small open-source microkernel on RISC-V, despite RISC-V's growing role in the systems that most plausibly benefit from migration: edge AI inference nodes (Antmicro, 2020; seL4 Foundation, 2025), automotive zone controllers (BlackBerry, 2024; Roland Berger, 2024), and confidential compute enclaves (Gu et al., 2019; Confidential Computing Consortium, 2025).

### 1.2 Research Problem

The 1999 Milojičić survey explicitly stated that "process migration will continue to attract research independently of its success in market deployment" (Milojičić et al., 1999, p. 38) and that the obstacles to adoption were neither algorithmic nor architectural but contextual: missing applications, missing infrastructure, and a deployment environment in which migration was not needed (Milojičić et al., 1999, §7.3). Since 2020, that context has changed: live migration is in production at hyperscale through container checkpoint-restore (Reber, 2023) and virtual-machine vMotion (Nelson, Lim and Hutchins, 2005), microkernels are in production in mass-market consumer hardware (BlackBerry, 2024), and RISC-V is the open ISA on which both edge AI (Antmicro, 2020) and software-defined-vehicle stacks (Avassa, 2026) are being built. The question of whether the 1999 barriers still hold, and what process migration looks like when it is rebuilt on contemporary microkernel discipline and open hardware, is therefore open. Specifically, prior microkernel migration work placed substantial mechanism inside the kernel — Mach's Distributed Memory Management (DMM) module exceeded 24,000 lines and its Distributed IPC (DIPC) module exceeded 32,000 lines (Milojičić et al., 1999, p. 29) — contradicting the modern microkernel principle of placing policy in user space and only minimal mechanism in the kernel (Liedtke, 1995; Heiser et al., 2006). Whether process migration can be redesigned as a user-space service on a minimal microkernel — and what the runtime cost of doing so is on RISC-V hardware — is not answered in the literature.

### 1.3 Objectives of the Project

The objectives of this project are:

1. **To design and implement** a process-migration mechanism as a user-space service on a minimal RISC-V microkernel, exposing only checkpoint and restore primitives at the kernel boundary so that transport, target selection, serialisation, and policy reside in user space, in conformance with the policy-mechanism separation principle established by Liedtke (1995) and refined by Heiser et al. (2006).
2. **To measure** the migration latency, throughput, and kernel trusted-computing-base size of the implementation on RISC-V (QEMU `virt` platform) using representative workloads, providing data points that do not exist in the literature for this architecture (Milojičić et al., 1999; seL4 Foundation, 2025).
3. **To compare** the user-space migration mechanism against a kernel-resident migration path on the same kernel, quantifying the overhead of relocating migration mechanism out of the kernel — a comparison performed by Mach (Milojičić et al., 1993) on 1990s hardware but not repeated on modern open hardware.
4. **To re-evaluate** the four non-technical adoption barriers identified by Milojičić et al. (1999, §7.3) against 2025 deployment realities, identifying which barriers have been lifted by industry change (Reber, 2023; BlackBerry, 2024; Avassa, 2026) and which remain.
5. **To produce** an open-source reference artefact (`Hydra`) so that subsequent researchers working on RISC-V microkernel distribution have a published baseline, addressing the absence of such an artefact noted in recent microkernel research literature (Chen et al., 2024; Gu et al., 2020).

### 1.4 Research Questions

This proposal addresses four research questions:

- **RQ1.** Can transparent process migration be implemented as a user-space service over a minimal RISC-V microkernel using only checkpoint and restore kernel primitives, while preserving correctness for representative compute workloads (Milojičić et al., 1999; Liedtke, 1995)?
- **RQ2.** What is the runtime cost — measured as migration latency, freeze time, and throughput on representative workloads — of placing migration mechanism in user space rather than in the kernel, on the same kernel and the same RISC-V hardware (Milojičić et al., 1993; Gu et al., 2020)?
- **RQ3.** Which of the four adoption barriers identified by Milojičić et al. (1999, §7.3) remain valid in 2025, given the emergence of container live migration (Reber, 2023), software-defined vehicles (BlackBerry, 2024; Avassa, 2026), edge AI inference (Antmicro, 2020), and confidential compute (Gu et al., 2019)?
- **RQ4.** Does the open-source reference artefact produced by this work provide a sufficient baseline for subsequent investigation of fault-tolerance, security, and policy questions on RISC-V microkernels (Klein et al., 2009; seL4 Foundation, 2025)?

### 1.5 Scope of the Research

The project is scoped to the following:

**In scope.** A single-architecture (RISC-V 64-bit, `rv64gc`) microkernel implementation (Waterman and Asanović, 2019); user-space migration over TCP/IP using the QEMU `virt` platform (Bellard, 2005) with VirtIO networking (Russell, 2008); one primary compute workload (a parallel raytracer) and one I/O-mixed workload (an HTTP server); empirical comparison of two migration paths — kernel-resident and user-space — on the same kernel; and a critical re-evaluation of the four §7.3 barriers from Milojičić et al. (1999) supported by recent industry literature (BlackBerry, 2024; Reber, 2023; Avassa, 2026).

**Out of scope.** Formal verification of the kernel — this is not a seL4-replacement effort (Klein et al., 2009); production-grade security hardening; multi-core scheduling and cross-core migration (Boyd-Wickizer et al., 2010); heterogeneous-ISA migration (Barbalace et al., 2017); persistence and storage migration; live migration of stateful network connections; and any claim to algorithmic novelty in checkpoint/restore mechanism itself, which Milojičić et al. (1999, §7.2) explicitly classify as a solved problem. The contribution is an artefact, a set of measurements, and a context-driven re-evaluation, not a new theory of distributed operating systems.

---

## Chapter 2: Literature Review

The literature on process migration spans four decades. This chapter surveys the body of prior work that bounds the present project, identifies the specific gap it addresses, and concludes with the position taken in this proposal.

### 2.1 Findings by Other Researchers

The canonical reference is Milojičić et al. (1999), republished as Milojičić et al. (2000) in ACM Computing Surveys, which classifies migration implementations into eight categories — early work, transparent migration in UNIX-like systems, message-passing OS, microkernels, user-space migrations, application-specific migration, mobile objects, and mobile agents — and presents detailed case studies of MOSIX, Sprite, Mach, and LSF (Milojičić et al., 1999, §5). The survey explicitly places Amoeba (Mullender et al., 1990; Tanenbaum et al., 1990), Mach (Accetta et al., 1986), Chorus (Rozier, 1992), BirliX (Härtig, Kowalski and Kühnhauser, 1993), RHODOS, and Arcade (Cohn, Delaney and Tracey, 1989) in the microkernel migration category and concludes that "microkernels also support migration more easily because of simpler abstractions… however, extensive complexity is introduced for supporting distributed IPC and distributed memory management" (Milojičić et al., 1999, p. 32). Earlier surveys by Smith (1988), Eskicioglu (1990), and Nuttal (1994) had similarly identified microkernel architectures as natural hosts for migration mechanism but did not anticipate the discipline of policy-mechanism separation later articulated by Liedtke (1995).

Mach implemented both user-level and kernel migration on the same microkernel (Milojičić et al., 1993). The simplified user-level migration server was approximately 400 lines of code; the optimised version reached several thousand lines; the supporting Distributed IPC (DIPC) and Distributed Memory Management (DMM) modules together exceeded 56,000 lines (Milojičić et al., 1999, p. 29). Performance was measured on three Intel 33 MHz 80486 PCs with 8 MB of RAM. Milojičić et al. (1999, p. 29) explicitly draw the lesson that placing migration in the kernel via DIPC/DMM "entails most of the complexity and… limits performance and fault resilience." MOSIX (Barak and Litman, 1985; Barak and Wheeler, 1989) and Sprite (Douglis and Ousterhout, 1991) both implemented kernel-resident migration on UNIX-like systems; LSF (Zhou et al., 1994) used user-level checkpoint-restart via Condor (Litzkow and Solomon, 1992). More recent work has continued in adjacent directions — heterogeneous-ISA process migration on Linux (Barbalace et al., 2017), distributed multi-threaded checkpointing (Ansel, Arya and Cooperman, 2009), and self-migration of operating systems (Hansen and Henriksen, 2002) — but none on a microkernel. Critically, the four-decade barrier identified in Milojičić et al. (1999, §7) was non-technical: a lack of killer applications, a lack of distributed-OS infrastructure in the marketplace, the availability of simpler alternatives such as remote invocation (Birrell and Nelson, 1984), and sociological resistance to processes "visiting" personal workstations (Krueger and Chawla, 1991; Douglis and Ousterhout, 1991).

Since 1999, the deployment landscape has shifted substantially. Container live migration via CRIU (Berrange, 2014) is now used in production at hyperscale under Kubernetes and Docker, with documented use cases including stateful-workload rebalancing and disaster recovery (Reber, 2023; Red Hat, 2023). VMware vMotion has performed live virtual-machine migration in datacentres since the mid-2000s (Nelson, Lim and Hutchins, 2005; Clark et al., 2005). The seL4 microkernel (Klein et al., 2009) achieved end-to-end functional correctness proof, with subsequent extensions to information-flow security (Murray et al., 2013) and timing-channel protection (Heiser, Klein and Murray, 2019), and has been ported to RISC-V (seL4 Foundation, 2025; Hensoldt Cyber, 2023). QNX reports deployment in over 235 million vehicles and meets ISO 26262 ASIL D (BlackBerry, 2024). Google's KataOS is built on seL4 for embedded ambient computing (Google, 2022). The Software-Defined Vehicle industry has explicitly committed to distributed microkernel architectures with inter-zone workload migration as a target (BlackBerry, 2024; Avassa, 2026; Connected Automated Driving, 2025; Roland Berger, 2024). The DARPA PROVERS / INSPECTA programme has selected seL4 plus the HAMR framework from Kansas State University as its model-based development chain (Collins Aerospace, 2025). RISC-V edge AI research (Antmicro, 2020; seL4 Foundation, 2025) has begun porting microkernels to AI-class SoCs for confidential edge inference. The global microkernel real-time operating system market is forecast to grow substantially through 2030 (Research and Markets, 2025).

Recent microkernel performance work — UnderBridge (Gu et al., 2020) and ChCore (Chen et al., 2024) — has reduced microkernel IPC overheads to within 5% of monolithic kernels for IPC-intensive workloads. The MicroTEE work (Gu et al., 2019) and the RISC-V AP-TEE standardisation effort (RISC-V International, 2025; Confidential Computing Consortium, 2025) place microkernels at the centre of contemporary trusted-execution-environment design, with active deployment in GPU-attached confidential inference (NVIDIA, 2024). Surveys of RISC-V secure enclaves (Lu et al., 2025) and dependable RISC-V edge cores (Pinto and Garlati, 2023) confirm the architecture's centrality to safety-critical and security-critical computing. None of these contemporary works, however, addresses process migration, and none of the existing RISC-V microkernels (seL4, KataOS, Zephyr) ship a migration mechanism.

### 2.2 The Research Gap

Three observations frame the gap.

First, **no peer-reviewed reference implementation of process migration exists for an open-source RISC-V microkernel.** Mach migration was reported on x86 (Milojičić et al., 1993); MOSIX on UNIX/x86 (Barak and Litman, 1985); Sprite on SPARC (Douglis and Ousterhout, 1991); CRIU is Linux-only (Berrange, 2014; Reber, 2023). The seL4 RISC-V port (seL4 Foundation, 2025; Hensoldt Cyber, 2023) has no migration mechanism. The RISC-V instruction set is now the strategic open hardware target for edge AI (Antmicro, 2020) and SDV (BlackBerry, 2024; RISC-V International, 2025), yet there is no published artefact, no kernel-vs-userspace comparison, and no measurement of migration latency for any open microkernel on this architecture.

Second, **prior microkernel migration work contradicts modern microkernel discipline.** Mach's DIPC and DMM placed tens of thousands of lines of distribution-related code inside or adjacent to the kernel (Milojičić et al., 1999, p. 29). Heiser et al. (2006), Liedtke (1995), and the seL4 design philosophy (Klein et al., 2009) argue that policy belongs in user space and only minimal mechanism belongs in the kernel. No published work has re-implemented process migration in conformance with this discipline on RISC-V, nor measured what is paid to do so, despite recent calls to revisit the trade-off in the context of contemporary microkernel performance optimisations (Gu et al., 2020; Chen et al., 2024).

Third, **the 1999 adoption-barrier analysis has not been revisited in light of post-2020 deployment realities.** Container live migration (Reber, 2023), automotive workload migration (BlackBerry, 2024; Roland Berger, 2024), edge AI offload (Antmicro, 2020), and confidential-VM mobility (Gu et al., 2019; NVIDIA, 2024) are all post-2020 industry developments that did not exist when Milojičić et al. (1999) wrote §7. A re-evaluation of which §7.3 barriers remain valid — and which have been lifted — is absent from the literature.

The gap addressed by this project is therefore the intersection of these three: **a user-space process-migration reference implementation on a minimal RISC-V microkernel, with empirical measurements of kernel-resident versus user-space placement on the same kernel, and a critical re-evaluation of the 1999 adoption barriers against 2025 deployment reality.**

### 2.3 Chapter Conclusion

This chapter has shown that process migration is a mature research area whose technical problems were largely solved by the mid-1990s (Milojičić et al., 1999, §7.2), and whose limited adoption was attributed by Milojičić et al. (1999, §7.3) to four non-technical barriers. It has shown that those barriers have been materially transformed by industry developments between 1999 and 2025 (Reber, 2023; BlackBerry, 2024; Avassa, 2026; Research and Markets, 2025), that the architectural locus of contemporary systems work has shifted to RISC-V (RISC-V International, 2025; Hensoldt Cyber, 2023), and that no published reference implementation, measurement, or kernel-versus-userspace comparison exists for process migration on a small open-source RISC-V microkernel. The contribution proposed here is consequently artefactual and empirical rather than algorithmic, and it positions itself explicitly as a re-evaluation against the canonical 1999 survey (Milojičić et al., 1999) rather than as a claim of novel mechanism.

---

## Chapter 3: Methodology

This project follows a *design-and-measure* methodology with four phases.

**Phase 1 — Implementation.** A minimal RISC-V microkernel (`Hydra`) targeting QEMU `virt` (Bellard, 2005) and `rv64gc` (Waterman and Asanović, 2019) will be implemented in C and assembly. The kernel will expose only the following primitives relevant to migration: thread suspension, address-space state export, address-space state import, and message-passing IPC (Liedtke, 1995). All policy — target selection, serialisation, network transport, retry, security — will reside in a user-space migration server, following the discipline established by Heiser et al. (2006) and Klein et al. (2009). A baseline kernel-resident migration path will be retained for comparison purposes, mirroring the dual-implementation approach taken by Milojičić et al. (1993) on Mach. Two workloads will be supported: a parallel ray-tracer (CPU-bound) and a single-threaded HTTP server (I/O-mixed). User-space libraries (`libc`, `libnet`, `libhydra`) and a VirtIO-Net driver (Russell, 2008) will provide the application substrate.

**Phase 2 — Measurement.** Each migration path (kernel-resident and user-space) will be benchmarked under identical workload, identical kernel, and identical QEMU configuration. The measured quantities follow established practice in the field (Milojičić et al., 1993; Gu et al., 2020; Chen et al., 2024): total migration latency (request → resume on destination); freeze time (period during which the process is suspended); state-transfer throughput as a function of address-space size; kernel TCB size in lines of code attributable to migration; and number of system calls invoked per migration. Each measurement will be repeated to obtain mean and 95% confidence interval, following standard performance-evaluation practice (Jain, 1991). The QEMU `virt` platform's deterministic timing facilities will be used to remove host scheduling noise. The result is a four- to six-row comparison table that does not currently exist in the literature for this architecture.

**Phase 3 — Re-evaluation of the §7.3 barriers.** A structured re-evaluation of the four Milojičić et al. (1999, §7.3) barriers (lack of applications, lack of infrastructure, alternative-simpler approaches, sociological resistance) will be conducted using post-2020 industry evidence: container-live-migration deployment data (Reber, 2023; Red Hat, 2023), automotive microkernel deployment data (BlackBerry, 2024; Roland Berger, 2024), seL4 deployment in DARPA INSPECTA (Collins Aerospace, 2025; seL4 Foundation, 2025), and software-defined-vehicle roadmaps (Avassa, 2026; Connected Automated Driving, 2025). The re-evaluation will be presented as a barrier-by-barrier analysis with explicit 1999-versus-2025 comparison, following the structured-review methodology articulated by Kitchenham (2004) for evidence-based software engineering.

**Phase 4 — Artefact release.** The `Hydra` source code, build instructions, benchmark scripts, and measurement data will be released under an open-source licence following the open-research artefact practice now standard in systems venues (ACM, 2020). The intent is that future work on fault tolerance, security, or policy on RISC-V microkernels can adopt `Hydra` as a starting artefact rather than re-implementing the substrate.

**Threats to validity.** The principal validity threat is that QEMU is not real hardware: measured latencies may not reflect physical-RISC-V deployment (Bellard, 2005; Patel et al., 2011). This will be mitigated by reporting both wall-clock and cycle-count measurements, and by presenting results as ratios (user-space versus kernel-resident on the same platform) rather than as absolute claims, following the validity-mitigation practice recommended by Wohlin et al. (2012). A secondary threat is that two workloads do not span the full migration design space (Milojičić et al., 1999, §2.4); this is acknowledged as a scope boundary and discussed in the dissertation's limitations chapter.

---

## List of References

Accetta, M., Baron, R., Bolosky, W., Golub, D., Rashid, R., Tevanian, A. and Young, M. (1986) 'Mach: A New Kernel Foundation for UNIX Development', *Proceedings of the USENIX Summer Conference*, pp. 93–112.

ACM (2020) *Artifact Review and Badging — Version 1.1*. Association for Computing Machinery. Available at: https://www.acm.org/publications/policies/artifact-review-and-badging-current (Accessed: 14 May 2026).

Ansel, J., Arya, K. and Cooperman, G. (2009) 'DMTCP: Transparent Checkpointing for Cluster Computations and the Desktop', *Proceedings of the IEEE International Symposium on Parallel and Distributed Processing*, pp. 1–12.

Antmicro (2020) *Building RISC-V Systems on the Secure seL4 Microkernel in Renode*. Available at: https://antmicro.com/blog/2020/09/sel4-on-riscv-in-renode/ (Accessed: 14 May 2026).

Avassa (2026) *Software-Defined Vehicle Platform: 2026 Development Guide*. Available at: https://avassa.io/articles/key-considerations-for-a-software-defined-vehicle-development-platform/ (Accessed: 14 May 2026).

Barak, A. and Litman, A. (1985) 'MOS: A Multicomputer Distributed Operating System', *Software — Practice and Experience*, 15(8), pp. 725–737.

Barak, A. and Wheeler, R. (1989) 'MOSIX: An Integrated Multiprocessor UNIX', *Proceedings of the USENIX Winter Conference*, pp. 101–112.

Barbalace, A., Karaoui, M.L., Wang, W., Xing, T., Olivier, P. and Ravindran, B. (2017) 'Edge Computing: The Case for Heterogeneous-ISA Container Migration', *Proceedings of the 16th ACM SIGPLAN/SIGOPS International Conference on Virtual Execution Environments (VEE '20)*, pp. 73–87.

Bellard, F. (2005) 'QEMU, a Fast and Portable Dynamic Translator', *Proceedings of the USENIX Annual Technical Conference, FREENIX Track*, pp. 41–46.

Berrange, D. (2014) 'CRIU: Checkpoint and Restore in Userspace', *LWN.net*. Available at: https://criu.org/ (Accessed: 14 May 2026).

Biggs, S., Lee, D. and Heiser, G. (2018) 'The Jury Is In: Monolithic OS Design Is Flawed — Microkernel-Based Designs Improve Security', *Proceedings of the 9th Asia-Pacific Workshop on Systems (APSys '18)*, pp. 1–7.

Birrell, A.D. and Nelson, B.J. (1984) 'Implementing Remote Procedure Calls', *ACM Transactions on Computer Systems*, 2(1), pp. 39–59.

BlackBerry (2024) *Software-Defined Vehicles: The Ultimate Guide*. Available at: https://blackberry.qnx.com/en/ultimate-guides/software-defined-vehicle (Accessed: 14 May 2026).

Boyd-Wickizer, S., Clements, A.T., Mao, Y., Pesterev, A., Kaashoek, M.F., Morris, R. and Zeldovich, N. (2010) 'An Analysis of Linux Scalability to Many Cores', *Proceedings of the 9th USENIX Symposium on Operating Systems Design and Implementation (OSDI '10)*, pp. 1–16.

Chen, H., Du, Y., Mi, Z. and Chen, R. (2024) 'Microkernel Goes General: Performance and Compatibility', *Proceedings of the 18th USENIX Symposium on Operating Systems Design and Implementation (OSDI '24)*.

Cheriton, D.R. (1988) 'The V Distributed System', *Communications of the ACM*, 31(3), pp. 314–333.

Clark, C., Fraser, K., Hand, S., Hansen, J.G., Jul, E., Limpach, C., Pratt, I. and Warfield, A. (2005) 'Live Migration of Virtual Machines', *Proceedings of the 2nd USENIX Symposium on Networked Systems Design and Implementation (NSDI '05)*, pp. 273–286.

Cohn, D.L., Delaney, W.P. and Tracey, K.M. (1989) 'Arcade: A Platform for Distributed Operating Systems', *Proceedings of the USENIX Workshop on Experiences with Distributed and Multiprocessor Systems*, pp. 373–390.

Collins Aerospace (2025) *INSPECTA: Integrated, Secure, Provable, Embedded Cyber Trusted Architecture*. seL4 Summit 2025 Abstracts. Available at: https://sel4.systems/Summit/2025/abstracts2025.html (Accessed: 14 May 2026).

Confidential Computing Consortium (2025) *Confidential Computing: The Next Frontier in Data Security*. White Paper, IDC #US53866125. Available at: https://confidentialcomputing.io/ (Accessed: 14 May 2026).

Connected Automated Driving (2025) *The Technology Roadmap 2025 for the Software-Defined Vehicle*. Available at: https://www.connectedautomateddriving.eu/blog/the-technology-roadmap-2025-for-the-software-defined-vehicle-sdvsw-platform-is-available/ (Accessed: 14 May 2026).

Douglis, F. and Ousterhout, J. (1991) 'Transparent Process Migration: Design Alternatives and the Sprite Implementation', *Software — Practice and Experience*, 21(8), pp. 757–785.

Eskicioglu, M.R. (1990) 'Design Issues of Process Migration Facilities in Distributed Systems', *IEEE Technical Committee on Operating Systems Newsletter*, 4(2), pp. 3–13.

Google (2022) *KataOS: An Open-Source Operating System for Ambient ML Applications*. Available at: https://opensource.googleblog.com/2022/10/announcing-kataos-and-sparrow.html (Accessed: 14 May 2026).

Gu, J., Hua, Z., Xia, Y., Chen, H. and Zang, B. (2019) 'MicroTEE: Designing TEE OS Based on the Microkernel Architecture', *Proceedings of the 18th IEEE International Conference on Trust, Security and Privacy in Computing and Communications (TrustCom '19)*, pp. 26–33.

Gu, J., Wu, X., Li, W., Liu, N., Mi, Z., Xia, Y. and Chen, H. (2020) 'Harmonizing Performance and Isolation in Microkernels with Efficient Intra-kernel Isolation and Communication', *Proceedings of the 2020 USENIX Annual Technical Conference*, pp. 401–417.

Hansen, J.G. and Henriksen, A.K. (2002) 'Nomadic Operating Systems', *Master's Thesis*, University of Copenhagen, DIKU.

Härtig, H., Kowalski, O.C. and Kühnhauser, W.E. (1993) 'The BirliX Security Architecture', *Journal of Computer Security*, 2(1), pp. 5–21.

Heiser, G. (2020) 'The seL4 Microkernel: An Introduction', *seL4 Foundation White Paper*. Available at: https://sel4.systems/About/seL4-whitepaper.pdf (Accessed: 14 May 2026).

Heiser, G., Elphinstone, K., Kuz, I., Klein, G. and Petters, S. (2006) 'Towards Trustworthy Computing Systems: Taking Microkernels to the Next Level', *ACM SIGOPS Operating Systems Review*, 41(4), pp. 3–11.

Heiser, G., Klein, G. and Murray, T. (2019) 'Can We Prove Time Protection?', *Proceedings of the Workshop on Hot Topics in Operating Systems (HotOS '19)*, pp. 23–29.

Hensoldt Cyber (2023) *MiG-V: The First Commercial RISC-V CPU with seL4 Support*. Available at: https://hensoldt-cyber.com (Accessed: 14 May 2026).

Jain, R. (1991) *The Art of Computer Systems Performance Analysis: Techniques for Experimental Design, Measurement, Simulation, and Modeling*. New York: John Wiley & Sons.

Kitchenham, B. (2004) *Procedures for Performing Systematic Reviews*. Technical Report TR/SE-0401, Keele University.

Klein, G., Elphinstone, K., Heiser, G., Andronick, J., Cock, D., Derrin, P., Elkaduwe, D., Engelhardt, K., Kolanski, R., Norrish, M., Sewell, T., Tuch, H. and Winwood, S. (2009) 'seL4: Formal Verification of an OS Kernel', *Proceedings of the 22nd ACM Symposium on Operating Systems Principles (SOSP '09)*, pp. 207–220.

Krueger, P. and Chawla, R. (1991) 'The Stealth Distributed Scheduler', *Proceedings of the 11th International Conference on Distributed Computing Systems (ICDCS '91)*, pp. 336–343.

Liedtke, J. (1995) 'On µ-Kernel Construction', *Proceedings of the 15th ACM Symposium on Operating Systems Principles (SOSP '95)*, pp. 237–250.

Litzkow, M. and Solomon, M. (1992) 'Supporting Checkpointing and Process Migration outside the UNIX Kernel', *Proceedings of the USENIX Winter Conference*, pp. 283–290.

Lu, Y., Wang, Q., Zhang, M. and Xu, F. (2025) 'A Survey of RISC-V Secure Enclaves and Trusted Execution Environments', *Electronics*, 14(21), Article 4171.

Milojičić, D., Zint, W., Dangel, A. and Giese, P. (1993) 'Task Migration on Top of the Mach Microkernel', *Proceedings of the Third USENIX Mach Symposium*, pp. 273–290.

Milojičić, D., Douglis, F., Paindaveine, Y., Wheeler, R. and Zhou, S. (1999) *Process Migration*. HP Laboratories Technical Report HPL-1999-21.

Milojičić, D., Douglis, F., Paindaveine, Y., Wheeler, R. and Zhou, S. (2000) 'Process Migration', *ACM Computing Surveys*, 32(3), pp. 241–299.

Mullender, S.J., van Rossum, G., Tanenbaum, A.S., van Renesse, R. and van Staveren, H. (1990) 'Amoeba — A Distributed Operating System for the 1990s', *IEEE Computer*, 23(5), pp. 44–53.

Murray, T., Matichuk, D., Brassil, M., Gammie, P., Bourke, T., Seefried, S., Lewis, C., Gao, X. and Klein, G. (2013) 'seL4: From General Purpose to a Proof of Information Flow Enforcement', *Proceedings of the 34th IEEE Symposium on Security and Privacy*, pp. 415–429.

Nelson, M., Lim, B.-H. and Hutchins, G. (2005) 'Fast Transparent Migration for Virtual Machines', *Proceedings of the USENIX Annual Technical Conference*, pp. 391–394.

Nuttal, M. (1994) 'A Brief Survey of Systems Providing Process or Object Migration Facilities', *ACM SIGOPS Operating Systems Review*, 28(4), pp. 64–80.

NVIDIA (2024) *Confidential Computing on NVIDIA Hopper GPUs*. Technical Brief. Available at: https://www.nvidia.com/en-us/data-center/solutions/confidential-computing/ (Accessed: 14 May 2026).

Patel, A., Daftedar, M., Shalan, M. and El-Kharashi, M.W. (2011) 'Embedded Hypervisor Xvisor: A Comparative Analysis', *Proceedings of the 23rd Euromicro International Conference on Parallel, Distributed and Network-Based Processing*, pp. 682–691.

Pinto, S. and Garlati, C. (2023) 'Towards Dependable RISC-V Cores for Edge Computing Devices', *Proceedings of the IEEE International Symposium on On-Line Testing and Robust System Design (IOLTS '23)*, pp. 1–7.

Rashid, R. and Robertson, G. (1981) 'Accent: A Communication Oriented Network Operating System Kernel', *Proceedings of the 8th ACM Symposium on Operating Systems Principles (SOSP '81)*, pp. 64–75.

Reber, A. (2023) 'Checkpoint/Restore in Linux: From CRIU to Container Migration', *Red Hat Developer Blog*. Available at: https://developers.redhat.com/blog/checkpoint-restore-criu (Accessed: 14 May 2026).

Red Hat (2023) *From Checkpoint/Restore to Container Migration*. Available at: https://www.redhat.com/en/blog/checkpointrestore-container-migration (Accessed: 14 May 2026).

Research and Markets (2025) *Microkernel Real-Time Operating System Market — Global Forecast Report 2025–2030*. Available at: https://www.researchandmarkets.com/ (Accessed: 14 May 2026).

RISC-V International (2025) *RISC-V Annual Report 2025*. Available at: https://riscv.org/wp-content/uploads/2026/01/RISC-V-Annual-Report-2025.pdf (Accessed: 14 May 2026).

Roland Berger (2024) *Explaining Software-Defined Vehicles*. Available at: https://www.rolandberger.com/en/Insights/Publications/Explaining-software-defined-vehicles.html (Accessed: 14 May 2026).

Rozier, M. (1992) 'Chorus (Overview of the Chorus Distributed Operating System)', *Proceedings of the USENIX Workshop on Micro-Kernels and Other Kernel Architectures*, pp. 39–70.

Russell, R. (2008) 'virtio: Towards a De-Facto Standard for Virtual I/O Devices', *ACM SIGOPS Operating Systems Review*, 42(5), pp. 95–103.

seL4 Foundation (2025) *seL4 Summit 2025 Abstracts*. Available at: https://sel4.systems/Summit/2025/abstracts2025.html (Accessed: 14 May 2026).

Smith, J.M. (1988) 'A Survey of Process Migration Mechanisms', *ACM SIGOPS Operating Systems Review*, 22(3), pp. 28–40.

Tanenbaum, A.S. and Bos, H. (2014) *Modern Operating Systems*. 4th edn. Upper Saddle River, NJ: Pearson.

Tanenbaum, A.S., Kaashoek, M.F., van Renesse, R. and Bal, H.E. (1990) 'The Amoeba Distributed Operating System — A Status Report', *Computer Communications*, 14(6), pp. 324–335.

Waterman, A. and Asanović, K. (2019) *The RISC-V Instruction Set Manual, Volume I: Unprivileged ISA*. Version 20191213. Available at: https://riscv.org/specifications/ (Accessed: 14 May 2026).

Watson, R.N.M., Neumann, P.G., Woodruff, J., Roe, M., Almatary, H., Anderson, J., Baldwin, J., Chisnall, D., Clarke, J., Davis, B., Eisen, L., Filardo, N.W., Grisenthwaite, R., Joannou, A., Laurie, B., Markettos, A.T., Moore, S.W., Murdoch, S.J., Nienhuis, K., Norton, R., Richardson, A., Rugg, P., Sewell, P., Son, S. and Xia, H. (2020) *Capability Hardware Enhanced RISC Instructions: CHERI Instruction-Set Architecture (Version 8)*. Technical Report UCAM-CL-TR-951, University of Cambridge.

Wohlin, C., Runeson, P., Höst, M., Ohlsson, M.C., Regnell, B. and Wesslén, A. (2012) *Experimentation in Software Engineering*. Berlin: Springer.

Zhou, S., Zheng, X., Wang, J. and Delisle, P. (1994) 'Utopia: A Load Sharing Facility for Large, Heterogeneous Distributed Computer Systems', *Software — Practice and Experience*, 23(12), pp. 1305–1336.
