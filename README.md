## libalac [![Build Status](https://secure.travis-ci.org/stephank/node-libalac.png)](http://travis-ci.org/stephank/node-libalac)

Bindings to the official Apple Lossless (ALAC) encoder and decoder.

### Installing

    $ npm install libalac

### Usage

The encoder is a regular stream:

    var alac = require('libalac');
    input.pipe(alac.encoder()).pipe(output);

`alac.encoder()` can also have an object argument with the following options:

 - `sampleRate`, defaults to `44100`
 - `channels`, defaults to `2`
 - `bitDepth`, defaults to `16`
 - `framesPerPacket`, defaults to `4096`

### Hacking the code

    git clone https://github.com/stephank/libalac.git
    cd libalac
    npm install
    npm test
