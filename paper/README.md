# Conference paper

IEEE two-column conference format, built as .docx.

    .venv/bin/python bench/paper_figs.py     # regenerate figures (600 dpi)
    .venv/bin/python paper/build_paper.py    # rebuild the .docx
    ./paper/make_pdf.sh                      # export the .pdf

`make_pdf.sh` is needed because a plain `soffice --convert-to pdf`
downsamples the figures to 300 ppi and re-encodes them as JPEG, which
puts ringing around the thin lines, hatching and 7 pt axis text. The
script disables both and then checks that every font is embedded and
that no figure came out JPEG-encoded.

Note on fonts: the .docx specifies Times New Roman. This machine does
not have it, so the PDF embeds Liberation Serif, which is metrically
identical -- the layout is the same to the pixel, but the embedded font
is not literally Times New Roman. Exporting from Word on a machine that
has the font will embed the real thing if a venue requires it.

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
