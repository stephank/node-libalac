## libalac [![Build Status](https://secure.travis-ci.org/stephank/node-libalac.png)](http://travis-ci.org/stephank/node-libalac)

Bindings to the official Apple Lossless (ALAC) encoder and decoder.

### Installing

    $ npm install libalac

### Usage

The encoder is a regular stream:

    var alac = require('libalac');
    var enc = alac.encoder();
    input.pipe(enc).pipe(output);

`alac.encoder()` can also have an object argument with the following options:

 - `sampleRate`, defaults to `44100`
 - `channels`, defaults to `2`
 - `bitDepth`, defaults to `16`
 - `framesPerPacket`, defaults to `4096` (usually no need to modify this)

The encoder object also has the following properties:

 - `cookie`, a buffer containing the ALAC magic cookie. These are parameters
   for the decoder, and is what you'd place in e.g. the `kuki` chunk of a
   CAF-file.

 - `packets`, array of offsets of packet starts. This array is only ever
   pushed to, and can be modified, or even replaced with an array-like object,
   as long as it has a `push` method.

### Hacking the code

    git clone https://github.com/stephank/libalac.git
    cd libalac
    npm install
    npm test
