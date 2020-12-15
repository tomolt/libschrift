# Changelog

## v0.9.1
For any user of the library, nothing should have changed at al since v0.9.0, except that
performance should be noticeably improved.

- Cleaned up the internals. For example, the struct buffer row pointer array is now gone.
  Instead, we just use a flat cell array.
- More suitable tesselation heuristic. The old one was based on the distance of the middle
  control point to the halfway point between the two other control points.
  The new one is instead based on the area of the triangles that the three points generate.
- A lot of minor performance improvements building up to a relatively significant speedup.
  Mostly achieved by reducing branch mispredictions and reordering or removing pipeline hazards
  in the raycaster and the post-processor.

## v0.9.0
- Improved/fixed glyph positioning calculations.
- Perform less internal heap allocations when rendering small glyphs.
- Hardening against integer overflow bugs.
- Explained the details of `sftdemo`.
- Replaced the ill-suited aa-tree in `sftdemo` with a simple bitfield.

## v0.8.0
- Slightly changed the missing glyph interface (check the man page for details).
- Makefile config for building on OpenBSD.
- Less reliance on undefined behaviour.
- Ported the core libary & stress app to Microsoft Windows / MSVC.
- (Hopefully) correct left side bearing calculations.
- Wrapped the header in extern "C" so the library can be used from C++.

## v0.7.1
- Pushed quantization to a later phase.
  Should get rid of any quantization errors (i.e. not quite black backgrounds).
- Overall performance optimizations.

## v0.7.0
- Fixed a left side bearing issue.
- Fixed a situation where the `assert` in `decode_contours` could be triggered.
- Fields in `SFT_Char` that are not set by `sft_char` will now always be initialized with zeros.
- Documented `sft_linemetrics` and `sft_kerning` in the man page.
- All outlines are now decoded before tesselation and rasterization are done in a single pass each.
- Optimized `clip_points`
- Changed the internal outline data-structure to be index-based, which also allowed untangling the
  critical path into a series of small data transformations. This has a huge amount of benefits:
  * The internal architecture is now much nicer
  * Much better debuggability all-round
  * Many new optimization possibilities have opened up
  * Less memory consumption than before
  * A pleasant 15% speed-boost for `sft_char`!

This is the first version to have a changelog.
