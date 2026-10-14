# Conference manuscript

[Word manuscript](Silva_RISCV_Process_Migration_IEEE.docx) ·
[PDF manuscript](Silva_RISCV_Process_Migration_IEEE.pdf)

This separate IEEE-format manuscript predates the supervisor's latest report
corrections. It is retained with its original figures and building tools;
its claims and citations have not been revised or revalidated in this cleanup.
Use the [current dissertation](../report/README.md) for the reviewed findings.

From the repository root:

```bash
.venv/bin/python scripts/paper_figures.py
.venv/bin/python scripts/build_paper.py
./scripts/export_paper_pdf.sh
```

The first two commands require matplotlib and python-docx respectively.
The export requires LibreOffice; optional Poppler tools check image encoding
and font embedding. Outputs replace this manuscript and its `figures/` assets.
Do not use these tools to rebuild the current dissertation.
