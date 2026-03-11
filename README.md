# doc-converter

Document format converter. Lightweight pandoc alternative written in C with minimal dependencies.

Reads Markdown, HTML, DOCX, and Email as input. Writes to 25+ output formats including LaTeX, man pages, RST, Typst, DocBook, EPUB, and many wiki/markup formats.

## Build

```
make
make install    # installs to ~/.local/bin/
```

Optional: install zlib (`libz`) for DOCX, ODT, and EPUB support. It is auto-detected via pkg-config.

Optional: install `pangocairo` for advanced PDF output. This is auto-detected via pkg-config. In practice, `pangocairo` typically brings in Cairo, Pango, HarfBuzz, FreeType, and Fontconfig support through your system packages.

Without `pangocairo`, PDF output still works, but it uses the built-in fallback renderer with more limited typography and font support.

## Usage

```
doc-converter [OPTIONS] INPUT [OUTPUT]
```

Formats are auto-detected from file extensions. Text output goes to stdout; binary formats (DOCX, ODT, EPUB, PDF) require `-o`.

### Options

| Option | Description |
|---|---|
| `-f, --from FORMAT` | Input format (auto-detected if omitted) |
| `-t, --to FORMAT` | Output format (auto-detected from output extension) |
| `-o, --output FILE` | Output file (required for binary formats) |
| `-B, --max-bytes N` | Fail if input exceeds N bytes (default: 256M, 0 = unlimited) |
| `--pdf-font-text FAMILY` | Default PDF font family for all non-code text |
| `--pdf-font-serif FAMILY` | Serif font family for PDF output |
| `--pdf-font-sans FAMILY` | Sans-serif font family for PDF output |
| `--pdf-font-mono FAMILY` | Monospace font family for PDF output |
| `-S, --strict` | Fail on unsupported constructs instead of silently dropping them |
| `-h, --help` | Show help |

### Examples

```
doc-converter README.md -t html
doc-converter paper.md -o paper.pdf
doc-converter paper.md -o paper.pdf --pdf-font-text "Noto Serif" --pdf-font-mono "JetBrains Mono"
doc-converter paper.md -o paper.pdf --pdf-font-serif "Noto Serif" --pdf-font-sans "Noto Sans" --pdf-font-mono "JetBrains Mono"
doc-converter page.html -t rst
doc-converter input.md -o output.docx
doc-converter message.eml -t md
```

## Dependency Matrix

| Dependency | Status | Features enabled |
|---|---|---|
| C compiler | Required | Core converter build |
| zlib | Optional | DOCX input, DOCX output, ODT output, EPUB output |
| pangocairo | Optional | Advanced PDF renderer with embedded fonts, Unicode text shaping, and configurable PDF font families |

### PDF behavior by build

| Build configuration | PDF output | Unicode text | `--pdf-font-*` flags |
|---|---|---|---|
| Without `pangocairo` | Basic built-in renderer | Limited | Not available |
| With `pangocairo` | Cairo/Pango renderer | Full Unicode supported by selected fonts | Available |

### Notes on PDF dependencies

- The project currently detects `pangocairo` as the build switch for advanced PDF support.
- HarfBuzz, FreeType, and Fontconfig are not checked separately in the `Makefile`; they are expected to be available through the installed `pangocairo` development package on your system.
- If `pangocairo` is not present at build time, PDF generation still works through the fallback renderer, but embedded Unicode-capable fonts and PDF font-family selection are not supported.
- By default, non-code PDF text uses the serif family and code uses the monospace family. `--pdf-font-text` changes the default family for all non-code text, while `--pdf-font-serif` and `--pdf-font-sans` let you override those roles more precisely.

## Supported Formats

### Input (read)

| Format | Name(s) | Extensions |
|---|---|---|
| Markdown | md, markdown | .md, .markdown |
| HTML | html, htm | .html, .htm |
| DOCX | docx | .docx (requires zlib) |
| Email | eml, email | .eml |

### Output (write)

| Format | Name(s) | Extensions |
|---|---|---|
| Markdown | md, markdown | .md, .markdown |
| HTML | html, htm | .html, .htm |
| DOCX | docx | .docx (requires zlib) |
| ODT | odt | .odt (requires zlib) |
| EPUB | epub | .epub (requires zlib) |
| Plain Text | txt, text | .txt |
| RTF | rtf | .rtf |
| LaTeX | tex, latex | .tex, .latex |
| PDF | pdf | .pdf |
| JSON | json | .json |
| RST | rst | .rst |
| AsciiDoc | adoc, asciidoc | .adoc, .asciidoc |
| Org Mode | org | .org |
| Textile | textile | .textile |
| MediaWiki | mediawiki, wiki | .mediawiki |
| Creole | creole | .creole |
| DokuWiki | dokuwiki | .dokuwiki |
| Jira | jira, confluence | .jira |
| BBCode | bbcode | .bbcode |
| Gemtext | gemtext, gmi | .gmi |
| Djot | djot | .djot |
| Man/Troff | man, troff, groff | .1 - .9 |
| Texinfo | texinfo, texi | .texi, .texinfo |
| POD | pod | .pod |
| DocBook | docbook, xml | .xml |
| Typst | typst, typ | .typ |
| OPML | opml | .opml |
| Muse | muse | .muse |

## Dependencies

- C compiler (clang or gcc)
- Optional: zlib -- enables DOCX input and DOCX/ODT/EPUB output
- Optional: pangocairo -- enables advanced PDF output with embedded fonts, Unicode shaping, and configurable serif/sans/monospace font families
