var util = require('util');
var Transform = require('stream').Transform;
var binding = require('./build/Release/binding.node');


exports.framesPerPacket = binding.kALACDefaultFramesPerPacket;
exports.sampleRate = 44100;
exports.channels = 2;
exports.bitDepth = 16;


function ALACEncoder(options) {
  Transform.call(this, options);

  this.framesPerPacket = options.framesPerPacket || exports.framesPerPacket;
  this.sampleRate = options.sampleRate || exports.ampleRate;
  this.channels = options.channels || exports.channels;
  this.bitDepth = options.bitDepth || exports.bitDepth;

  this.bytesPerPacket = this.channels * (this.bitDepth >> 3) * this.framesPerPacket;

  this._enc = new binding.Encoder({
    framesPerPacket: this.framesPerPacket,
    sampleRate: this.sampleRate,
    channels: this.channels,
    bitDepth: this.bitDepth
  });

  this._streamPos = 0;
  this._partial = null;

  this.cookie = this._enc.cookie;
  this.packets = [];
}
util.inherits(ALACEncoder, Transform);

ALACEncoder.prototype._transform = function(chunk, encoding, done) {
  if (this._partial) {
    chunk = Buffer.concat([this._partial, chunk]);
    this._partial = null;
  }

  var pos = 0;
  var remainder = chunk.length;
  var enc = this._enc;
  var bpp = this.bytesPerPacket;
  var outSize = bpp + binding.kALACMaxEscapeHeaderBytes;
  while (remainder >= bpp) {
    var out = new Buffer(outSize);

    var end = pos + bpp;
    var frame = chunk.slice(pos, end);
    var bytes = enc.encode(frame, out);
    pos = end;
    remainder -= bpp;

    if (!bytes)
      continue;

    this.packets.push(this._streamPos);
    this._streamPos += bytes;

    if (bytes === outSize)
      this.push(out);
    else
      this.push(out.slice(0, bytes));
  }

  if (remainder !== 0)
    this._partial = chunk.slice(pos);

  done();
};

ALACEncoder.prototype._flush = function(done) {
  if (this._partial) {
    var outSize = this._partial.length + binding.kALACMaxEscapeHeaderBytes;
    var out = new Buffer(outSize);

    var bytes = this._enc.encode(this._partial, out);

    if (bytes === outSize)
      this.push(out);
    else if (bytes)
      this.push(out.slice(0, bytes));
  }

  done();
};

exports.encoder = function(options) {
  return new ALACEncoder(options || {});
};
