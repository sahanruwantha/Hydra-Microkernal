#!/usr/bin/env bash
#
# make_pdf.sh - export the paper to PDF.
#
# The default `soffice --convert-to pdf` is not good enough here: it
# downsamples images to 300 ppi and re-encodes them as JPEG, which puts
# ringing artefacts around the thin lines, hatching and 7 pt axis text in
# the figures. The filter options below turn that off, so the figures go
# in losslessly at their native ~500 ppi.
#
# Checks afterwards that every font is embedded and that no image came
# out JPEG-encoded, both of which submission systems care about.
#
set -euo pipefail

cd "$(dirname "$0")"
DOCX=Silva_RISCV_Process_Migration_IEEE.docx
PDF=${DOCX%.docx}.pdf

if [[ ! -f $DOCX ]]; then
    echo "ERROR: $DOCX not found. Run: .venv/bin/python paper/build_paper.py" >&2
    exit 1
fi

rm -f "$PDF"
soffice --headless \
    --convert-to 'pdf:writer_pdf_Export:{
        "UseLosslessCompression":{"type":"boolean","value":"true"},
        "ReduceImageResolution":{"type":"boolean","value":"false"},
        "MaxImageResolution":{"type":"long","value":"1200"},
        "ExportBookmarks":{"type":"boolean","value":"true"},
        "UseTaggedPDF":{"type":"boolean","value":"true"}}' \
    --outdir . "$DOCX" >/dev/null

[[ -f $PDF ]] || { echo "ERROR: conversion produced no PDF" >&2; exit 1; }

echo "wrote $PDF"
pdfinfo "$PDF" | awk '/^Pages|^Page size/{print "  "$0}'

if command -v pdffonts >/dev/null; then
    if pdffonts "$PDF" | tail -n +3 | awk '{print $(NF-3)}' | grep -qv yes; then
        echo "  WARNING: some fonts are not embedded" >&2
    else
        echo "  all fonts embedded"
    fi
fi

if command -v pdfimages >/dev/null; then
    n=$(pdfimages -list "$PDF" | grep -c jpeg || true)
    if [[ $n -gt 0 ]]; then
        echo "  WARNING: $n image(s) JPEG-encoded; figures will show artefacts" >&2
    else
        echo "  all figures losslessly encoded"
    fi
fi
