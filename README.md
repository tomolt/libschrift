libschrift
==========
*libschrift* is a lightweight TrueType font rendering library.

It can be seen as a much smaller, but more limited alternative to *FreeType2*.

Specifically, *libschrift* aims to:
- Be as suckless as possible.
  See: <https://www.suckless.org/philosophy/>
- Make correct (as in artifact-free, UTF-8 handling etc.)
  font rendering easy-to-use.
- Be reasonably secure, which especially means to not crash,
  leak memory / resources or expose major security
  vulnerabilities on corrupted / malicious / random input.

Limitations
-----------
- The only available text encoding is UTF-8.
  See: <http://www.utf8everywhere.org>
- Support for most TrueType (.ttf) and certain OpenType (.otf) fonts.
  No bitmap or PostScript fonts (for now).
- No hinting. Especially no auto-hinting like *FreeType2*.

Documentation
-------------
For documentation on how to use *libschrift* in your own programs,
refer to the (currently incomplete) *schrift(3)* man page,
the source code of the bundled example program *sftdemo*,
as well as the header file *schrift.h*.
