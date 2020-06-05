# Changelog

## v0.7.0
- Fixed a left side bearing issue.
- Fixed a situation where the `assert` in `decode_contours` could be triggered.
- Fields in `SFT_Char` that are not set by `sft_char` will now always be initialized with zeros.
- Documented `sft_linemetrics` and `sft_kerning` in the man page.
- All outlines are now decoded before tesselation and rasterization are done in a single pass each.
- Optimized `clip_points`

This is the first version to have a changelog.
