var fs = require('fs');
var path = require('path');
var test = require('tap').test;
var alac = require('../');

var sine_pcm = fs.readFileSync(path.join(__dirname, 'sine.pcm'));
var sine_alac = fs.readFileSync(path.join(__dirname, 'sine.alac'));

test('basic encode', function(t) {
  t.plan(1);

  var enc = alac.encoder();
  enc.end(sine_pcm);

  var chunks = [];
  enc.on('readable', function() {
    var chunk = enc.read();
    if (chunk)
      chunks.push(chunk);
  });
  enc.on('end', function() {
    var outb = Buffer.concat(chunks);
    t.same(outb, sine_alac, 'compressed output matches fixture');
  });
});
