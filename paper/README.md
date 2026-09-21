# Conference paper

IEEE two-column conference format, built as .docx.

    .venv/bin/python bench/paper_figs.py     # regenerate figures (600 dpi)
    .venv/bin/python paper/build_paper.py    # rebuild the paper

`build_paper.py` implements the IEEE "Manuscript Templates for Conference
Proceedings" layout directly (Letter, 0.625 in side margins, two 3.5 in
columns with a 0.25 in gutter, Times New Roman 10 pt body, single-column
title block). Section, figure and table numbering, caption placement and
the three-rule table style follow the same template.

`bench/paper_figs.py` draws every figure at the column width it will be
printed at, rather than rescaling the dissertation figures, so no figure
carries sub-7 pt type. The palette is validated for colour-vision
deficiency and every filled mark carries hatching so the figures survive
greyscale printing.

Figures are numbered in order of first reference in the text, as IEEE
requires. Every number in the paper comes from bench/results/, not from
hand entry.
