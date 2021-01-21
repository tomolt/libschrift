# schrift To-Do's
The following bullet points are listed in no particular order.

## Bugs
- Romanian S-comma (Unicode: `U+0219`) is apparently not displayed.
  `AnonPro` is a font that *should* have an outline for this character.

## API redesign
- Instead of scales and offsets, the user will be able to directly supply a 2x3 matrix. There will be some convenience macros
  for constructing such a matrix from simpler parameters, like from a uniform scale alone.
- Anything like `sft_char` should take a flags parameter, so the `flags` field in struct SFT can be removed.

## Features
- Kerning needs to be tested.
- `stress` should render a large range of code points to be more representative.
- We will probably need user-defined transformations for slanted text etc.
  This should be pretty easy now since we already pass generic affine linear transformation matrices around everywhere.
- We need an interface for subpixel rendering.
  Mattias Andrée quite early on proposed a possible API for this.
- There are some kerning features like minimum values that are not yet supported.
- There are some compound glyph features like grid-snapping that are not yet supported.
- We need an interface to support using an entire priority / codepoint-range based stack of fonts.
  For the longest time, I thought this could be done by a text shaping library sitting on top of schrift.
  Now I'm starting to see that it would be much more beneficial to implement this directly within libschrift.
- Consider internally switching to Q16.16 (or Q12.20) fixnums for most rational number representations.
  This is practically neccessary to get good performance on CPUs without FPU, but on any system that has one
  it will probably incur a big speed hit ...
- right-to-left text support.

## Demo Applications
- A separate demo that works on Windows / non-X11 platforms.
  SDL2 seems like a good idea.

## Documentation
- Example snippets in the man page.

## Code Quality
- Refactor `simple_outline`
- Refactor `tesselate_curves`
- `transform_points` probably should take the outline as the primary argument.
- `clip_points` probably should take the outline as the primary argument.
- The following new functions to map array problems to single instance problems:
  * `decode_contours` / `decode_contour`
- Think about using some internal type aliases to make the intentions behind the typing clearer.
- struct outline is responsible for a lot of allocations, which could at least partially be done on the stack.
