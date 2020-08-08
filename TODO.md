# schrift To-Do's
The following bullet points are listed in no particular order.

## Bugs
- Punctuation apparently doesn't line up properly for monospace fonts.
  Andor has the details.

## Features
- Kerning needs to be tested.
- `stress` should render a large range of code points to be more representative.
- We will probably need user-defined transformations for slanted text etc.
- We need an interface for subpixel rendering.
  Mattias Andrée already proposed a possible API for this!
- There are some kerning features like minimum values that are not yet supported.
- There are some compound glyph features like grid-snapping that are not yet supported.
- We need an interface to support using an entire priority / codepoint-range based stack of fonts.
  For the longest time, I thought this could be done by a text shaping library sitting on top of schrift.
  Now I'm starting to see that it would be much more beneficial to implement this directly within libschrift.

## Demo Applications
- The comments in `sftdemo` can still be improved.
- Rename `sftdemo` to `x11demo`.
- Supply a `gl3demo`.
- `sftdemo` should probably make use of kerning.
- Rewrite `sftdemo` as a literate program? Might be a good idea.

## Documentation
- Example snippets in the man page.

## Code Quality
- Refactor `simple_outline`
- Refactor `tesselate_curves`
- `transform_points` probably should take the outline as the primary argument.
- `clip_points` probably should take the outline as the primary argument.
- Perhaps rename `struct buffer` to `struct raster` again?
- The following new functions to map array problems to single instance problems:
  * `decode_contours` / `decode_contour`
- Consider internally switching to Q16.16 fixnums for most rational number representations.

