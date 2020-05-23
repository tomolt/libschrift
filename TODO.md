# schrift TODOs
The following bullet points are listed in no particular order.

## Bugs
- There might still be situations in which the asserts inside `decode_contours` get triggered.
  This should never happen and points to a bug somewhere in the outline decoder.

## Features
- Kerning needs to be tested.
- `sftdemo` needs a better sample text. The one I planned on using is apparently copyrighted ...
- `stress` should render a large range of code points to be more representative.
- `sftdemo`'s use of `aa_tree` *might* be a major performance bottleneck. If that's the case,
  we will need another algorithm / datastructure / strategy for loading code point ranges.
- We need an interface to check if the missing glyph is about to be rendered and conditionally abort,
  so the user can retry rendering with another font instead.
- `sftdemo` should operate on a hierarchy of fonts, since nowadays most fonts aren't monolithic files anymore.
- We need an interface to check if a font is monospace.
- We will probably need user-defined transformations for slanted text etc.
- `sftdemo` should probably make use of kerning.

## Documentation
- Fill in missing sections and paragraphs in the man page.
- Example snippets in the man page.
- The `sft_kerning` function has to be documented, preferably in the man page.
- The inner workings of `sftdemo` have to be better documented, as it is meant to be example code.

