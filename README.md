# doc-converter

Document format converter. Lightweight pandoc alternative written in C with minimal dependencies.

Reads Markdown, HTML, DOCX, and Email as input. Writes to 25+ output formats including LaTeX, man pages, RST, Typst, DocBook, EPUB, and many wiki/markup formats.

## Build

```
make
make install    # installs to ~/.local/bin/
```

Optional: install zlib (`libz`) for DOCX, ODT, and EPUB support. It is auto-detected via pkg-config.

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
| `-S, --strict` | Fail on unsupported constructs instead of silently dropping them |
| `-h, --help` | Show help |

### Examples

```
doc-converter README.md -t html
doc-converter paper.md -o paper.pdf
doc-converter page.html -t rst
doc-converter input.md -o output.docx
doc-converter message.eml -t md
```

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
