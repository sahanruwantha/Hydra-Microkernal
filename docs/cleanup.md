# Repository cleanup — 14 October 2026

The existing working-tree edits were preserved. No Git reset, commit, branch
change, or history rewrite was performed.

## Structure

- `kernal/` → `kernel/`; include paths and build rules updated.
- Compiler objects, dependency files, configuration stamp and executable → `build/`.
- Current supervisor-revised Word/PDF → `docs/report/Final_Report.docx` and `.pdf`.
- Sixteen current report figures extracted unchanged → `docs/report/figures/`.
- Original proposal → `docs/proposal/`; full reference survey → `docs/references/`.
- Separate conference manuscript → `docs/paper/`; building tools → `scripts/`.
- Testing and implementation history → `docs/`.
- Distributed launcher and log utility → `scripts/`; runtime logs → `build/logs/`.
- Retained benchmark logs and summaries stay in `bench/results/`.

## Removed from the active tree

Superseded report copies, chapter drafts and stale Markdown reports; older
Mermaid diagrams and standalone charts; the partial survey PDF; one-off Word
editing helpers; the original network-driver stub backup; the invalid
unfiltered cross-node summary; a sample debug log; six empty C source files;
and reproducible compiler/Python cache files.

The installed `.venv/` was retained for the document and plotting tools and
is ignored, as are local editor settings. Existing implementation files were
kept unless empty; this cleanup does not redesign kernel functionality.

## Recovery

Verified external backup (306 files, excluding Git internals and `.venv/`):

`/home/sahan/.codex/visualizations/2026/10/10/01a1238a-8813-72b1-a31d-acb4174507f3/repo-cleanup-backup-20261014`

- `before-cleanup.tar.gz`: pre-cleanup files, including untracked work.
- `manifest.json`: SHA-256 and size for every backed-up file.
- `working-tree.patch` and `git-status.txt`: tracked edits and status before cleanup.
- `cleanup-actions.json`: exact old/new paths and reasons for removal.

Restore individual files into a separate folder first to avoid overwriting
the organized repository. Historical tracked versions also remain in Git.

## Content scope

Current report Word/PDF files and retained benchmark evidence are unchanged.
The proposal and conference paper are earlier documents, clearly labelled
as such. This cleanup does not resolve the report's disclosed four-byte
historical checkpoint-size discrepancy or authenticate remaining citations.

## Verification

Both comparison modes compile and link without warnings. Switching KERNEL →
USERSPACE without cleaning selects the correct banner; an unchanged build
performs no compilation. The USERSPACE executable load image is byte-for-byte
identical to the pre-cleanup build (SHA-256 `733d609e0b0fd6c48bc146584e868833c75cd174d49710dcb59363153f5454a2`).

The two-node QEMU acceptance run passed: 60 local restores and one isolation
check per node, 20 peer frames per node, 15/15 supervisor migrations, 1/1
isolated migration, 16/16 acknowledgements, and zero errors. Local multicast
required execution outside the network-restricted sandbox.

All retained benchmark files, current report Word/PDF, original proposal,
reference survey, and conference manuscript/figures match their pre-cleanup
SHA-256 hashes. Seventy nonempty implementation files retain their contents,
with only kernel include paths adjusted. Six empty source files were removed.

Python and shell syntax, 42 local document links, three saved-data analysers,
three diagnostic plot tools, and the relocated launcher/log status commands
passed. The manuscript figure, Word and PDF builders passed in an isolated
temporary copy; the retained manuscript was not regenerated.

Active files excluding build products and local environments decreased from
306 pre-cleanup files (49.5 MiB, including old build products) to 140 files
(29.0 MiB). Validation logs are retained in the external backup’s `validation/`
folder. Pre-existing trailing whitespace in `server/net_srv.c` was left intact.
