libschrift
==========
*libschrift* is a lightweight and secure TTF font rendering library.

Goals
-----
- Be as suckless as possible.
  See: <https://www.suckless.org/philosophy/>
- Make correct (as in artifact-free, UTF-8 handling etc.)
  font rendering easy-to-use.
- Be reasonably secure, which especially means to not crash,
  leak memory / resources or expose major security
  vulnerabilities on corrupted / malicious / random input.

Rationale
---------
Besides *libschrift*, there really are only two cross-platform
open-source font rendering libraries available: *FreeType2* and
*stb\_truetype*. *FreeType2* is a pretty stable implementation
that supports most state-of-the-art features, but that also makes
it incredibly bloated and frustrating to use. *stb\_truetype* is
a lot better in this regard by virtue of keeping a much smaller
scope, but it suffers from some annoying problems that can lead to
visual artifacts. On top of that, it doesn't seem to be designed
to be particularly secure, as its main use-case is working on
trusted input files.

Limitations
-----------
- Only Latin, Cyrillic and Greek writing systems will be fully
  supported for now, as others would require significantly more
  support for OpenType-specific features.
- The only available text encoding is UTF-8.
  See: <http://www.utf8everywhere.org>
- Support for most TrueType (.ttf) and OpenType (.otf) fonts.
  No bitmap or PostScript fonts (for now).
- No hinting. Especially no auto-hinting like *FreeType2*.

Progress
--------
Although most logic is already in-place and working, *libschrift*
is not quite usable yet. Still missing are:

- Import / rewrite most code from libschrift1.
- Better documentation. At least some man pages.
- Compound glyph support.
- Correct handling of grapheme clusters?

