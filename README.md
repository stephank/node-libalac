## libalac [![Build Status](https://secure.travis-ci.org/stephank/node-libalac.png)](http://travis-ci.org/stephank/node-libalac)

Bindings to the official Apple Lossless (ALAC) encoder and decoder.

### Installing

    $ npm install libalac

### Encoder usage

The encoder is a regular stream:

    var alac = require('libalac');
    var enc = alac.encoder({
      sampleRate: 44100,
      channels: 2,
      bitDepth: 16
    });
    input.pipe(enc).pipe(output);

`alac.encoder()` must have an object argument, which can contain regular
readable and writable stream options, along with the following:

 - `sampleRate` (in Hz) *required*
 - `channels`, *required*
 - `bitDepth`, *rqeuired*
 - `framesPerPacket`, defaults to `4096` (usually no need to modify this)

The encoder object also has the following properties:

 - `cookie`, a buffer containing the ALAC magic cookie. These are parameters
   for the decoder, and is what you'd place in e.g. the `kuki` chunk of a
   CAF-file.

 - `packets`, array of sizes of packets in the stream. This array is only ever
   pushed to, and can be modified, or even replaced with an array-like object,
   as long as it has a `push` method.

 - `sampleRate`, `channels`, `bitDepth`, and `framesPerPacket` containing the
   final parameters used in the encoder.

Note that the encoder always reads input in native byte order!

### Hacking the code

    git clone https://github.com/stephank/libalac.git
    cd libalac
    npm install
    npm test
