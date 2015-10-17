var fs = require('fs');
var path = require('path');
var test = require('tap').test;
var alac = require('../');

var sine_pcm = fs.readFileSync(path.join(__dirname, 'sine.pcm'));
var sine_alac = fs.readFileSync(path.join(__dirname, 'sine.alac'));
var sine_kuki = fs.readFileSync(path.join(__dirname, 'sine.kuki'));
var sine_pakt = JSON.parse(fs.readFileSync(path.join(__dirname, 'sine.pakt')));

test('basic encode', function(t) {
  t.plan(2);

  var enc = alac.encoder({
    sampleRate: 44100,
    channels: 2,
    bitDepth: 16,
    framesPerPacket: alac.defaultFramesPerPacket
  });
  enc.end(sine_pcm);
  // FIXME: broken
  //t.same(enc.cookie, sine_kuki, 'cookie matches fixture');

  var chunks = [];
  enc.on('readable', function() {
    var chunk = enc.read();
    if (chunk)
      chunks.push(chunk);
  });
  enc.on('end', function() {
    var outb = Buffer.concat(chunks);
    t.same(outb, sine_alac, 'compressed output matches fixture');
    t.same(enc.packets, sine_pakt, 'packet table matches fixture');
  });
});

test('basic decode', function(t) {
  t.plan(1);

  var enc = alac.decoder({
    cookie: sine_kuki,
    channels: 2,
    bitDepth: 16,
    framesPerPacket: alac.defaultFramesPerPacket,
    packets: sine_pakt
  });
  enc.end(sine_alac);

  var chunks = [];
  enc.on('readable', function() {
    var chunk = enc.read();
    if (chunk)
      chunks.push(chunk);
  });
  enc.on('end', function() {
    var outb = Buffer.concat(chunks);
    t.same(outb, sine_pcm, 'decompressed output matches fixture');
  });
});
