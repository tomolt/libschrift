libschrift
==========
*libschrift* is a lightweight TrueType font rendering library.

It can be seen as a much smaller, but more limited alternative to *FreeType2*.

Specifically, *libschrift* aims to:
- Be as simple and easy-to-use as possible.
  See: <https://www.suckless.org/philosophy/>
- Make correct (as in artifact-free, Unicode-aware etc.)
  font rendering easy to achieve.
- Be reasonably secure, which especially means to not crash,
  leak memory / resources or expose major security
  vulnerabilities on corrupted / malicious / random inputs.

Limitations
-----------
- Unicode is the only supported text encoding.
- Support for most TrueType (.ttf) and certain OpenType (.otf) fonts.
  No bitmap or PostScript fonts.
- No hinting. Especially no auto-hinting like *FreeType2*.

Documentation
-------------
For documentation on how to use *libschrift* in your own programs,
refer to the *schrift(3)* man page,
the source code of the bundled *demo*,
as well as the header file *schrift.h*.

You can also view the man page in your browser at
<http://tomolt.github.io/libschrift/>

Progress
--------
In terms of security and performance, libschrift is already pretty solid.
However, it is still missing some important features, like right-to-left text support.

Visual Quality
--------------
A screenshot of the demo program:
![demo screenshot](resources/demo-screenshot.png)
