# schrift TODOs
The following bullet points are listed in no particular order.

## Bugs
There are currently no known bugs ...
But that doesn't mean there aren't any!

## Features
- Kerning needs to be tested.
- `sftdemo` needs a better sample text. The one I planned on using is apparently copyrighted ...
- `stress` should render a large range of code points to be more representative.
- `sftdemo`'s use of `aa_tree` *might* be a major performance bottleneck. If that's the case,
  we will need another algorithm / datastructure / strategy for loading code point ranges.
- We will probably need user-defined transformations for slanted text etc.
- `sftdemo` should probably make use of kerning.
- We need an interface for subpixel rendering.
  Mattias Andrée already proposed a possible API for this!
- There are some kerning features like minimum values that are not yet supported.
- There are some compound glyph features like grid-snapping that are not yet supported.
- We need an interface to support using an entire priority / codepoint-range based stack of fonts.
  For the longest time, I thought this could be done by a text shaping library sitting on top of schrift.
  Now I'm starting to see that it would be much more beneficial to implement this directly within libschrift.

## Documentation
- Fill in missing sections and paragraphs in the man page.
- Example snippets in the man page.
- The `sft_kerning` function has to be documented, preferably in the man page.
- The inner workings of `sftdemo` have to be better documented, as it is meant to be example code.

