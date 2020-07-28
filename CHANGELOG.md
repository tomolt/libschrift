# Changelog

## v0.9.0

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
