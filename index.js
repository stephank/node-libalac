var util = require('util');
var Transform = require('stream').Transform;
var binding = require('./build/Release/binding.node');


exports.defaultFramesPerPacket = binding.kALACDefaultFramesPerPacket;


function ALACEncoder(options) {
  Transform.call(this, options);

  if (typeof(options.sampleRate) !== 'number')
    throw new Error("Missing sample rate parameter");
  this.sampleRate = options.sampleRate;

  if (typeof(options.channels) !== 'number')
    throw new Error("Missing channels parameter");
  this.channels = options.channels;

  if (typeof(options.bitDepth) !== 'number')
    throw new Error("Missing bit depth parameter");
  this.bitDepth = options.bitDepth;

  this.framesPerPacket = options.framesPerPacket || exports.defaultFramesPerPacket;

  this._bytesPerPacket = this.channels * (this.bitDepth >> 3) * this.framesPerPacket;

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
  var bpp = this._bytesPerPacket;
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

    this.packets.push(bytes);
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

    this.packets.push(bytes);

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


function ALACDecoder(options) {
  Transform.call(this, options);

  if (!Buffer.isBuffer(options.cookie))
      throw new Error("ALAC decoder requires a cookie");

  if (typeof(options.channels) !== 'number')
    throw new Error("Missing channels parameter");
  if (typeof(options.bitDepth) !== 'number')
    throw new Error("Missing bit depth parameter");
  if (typeof(options.framesPerPacket) !== 'number')
    throw new Error("Missing frames per packet parameter");
  this._bytesPerFrame = options.channels * (options.bitDepth >> 3);
  this._bytesPerPacket = this._bytesPerFrame * options.framesPerPacket;

  this._dec = new binding.Decoder({
    cookie: options.cookie,
    channels: options.channels,
    framesPerPacket: options.framesPerPacket
  });
  this._packets = options.packets ? options.packets.slice(0) : [];
  this._buffers = [];
  this._available = 0;
  this._streamPos = 0;
}
util.inherits(ALACDecoder, Transform);

ALACDecoder.prototype._transform = function(chunk, encoding, done) {
  this._buffers.push(chunk);
  this._available += chunk.length;
  this._exhaust();
  done();
};

ALACDecoder.prototype.packets = function(arr) {
  this._packets = this._packets.concat(arr);
  this._exhaust();
};

ALACDecoder.prototype._exhaust = function() {
  if (this._packets.length === 0) return;

  var available = this._available;
  var needed = this._packets[0];
  if (available < needed) return;

  var outSize = this._bytesPerPacket;
  var frameSize = this._bytesPerFrame;
  var buf = Buffer.concat(this._buffers, this._available);
  var pos = 0;
  do {
    needed = this._packets[0];
    if (available < needed) return;

    this._packets.shift();
    var packet = buf.slice(pos, pos + needed);
    available -= needed;
    pos += needed;

    var out = new Buffer(outSize);
    var bytes = this._dec.decode(packet, out) * frameSize;

    if (bytes === outSize)
      this.push(out);
    else if (bytes)
      this.push(out.slice(0, bytes));
  } while (this._packets.length !== 0);

  this._buffers.length = 0;
  this._available = available;
  if (available !== 0)
    this._buffers.push(buf.slice(pos));
};

exports.decoder = function(options) {
  return new ALACDecoder(options);
};
