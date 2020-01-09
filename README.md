### What is this? ###

A program which converts [Tile World][]'s solution format, TWS, into a readable [JSON][]-based format.

[Tile World]: http://www.muppetlabs.com/~breadbox/software/tworld/
[JSON]: http://www.json.org/

### Is it any good? ###

Yes.

### Okay, how do i use it? ###

First compile it.

    % make

Then run

    % ./tws2json ~/.tworld/intro-ms.dac.tws

You should see something like this:

    {"class":"tws",
     "ruleset":"ms",
     "levelset":"intro-ms.dac",
     "generator":"tws2json/0.1",
     "solutions":[
      {"class":"solution",
       "number":1,
       "password":"BDHP",
       "rndslidedir":1,
       "stepping":0,
       "rndseed":2017730254,
       "moves":"5L3R3,r,,4Rr,,R2Ll,,2L5D5L3D3U5R2D5R3U3D5L,Dd,,R,r,,R,l,,5L3R6D,,d"},
      {"class":"solution",
       "number":2,
    ...

### Format ###

Pretty much the above.

The movestring is based on the [notation][] commonly used by players. See [format.txt](format.txt) for more details.

The format is still in flux though, so don't get too comfortable.

[notation]: https://wiki.bitbusters.club/Directional_notation

### Bugs ###

Conversion from JSON to TWS isn't yet supported.

Mouse moves are not yet supported.

### License ###

GPLv2. See [COPYING](COPYING) for the full license text.

Several files have been borrowed from [Tile World][], copyright Brian Raiter, which is also licensed under the GPL.

[bstrlib][] was written by Paul Hsieh, and is licensed under the GPL.

[bstrlib]: http://bstring.sourceforge.net/
