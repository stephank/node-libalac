// ALAC.node, © 2014 Stéphan Kochen
// MIT-licensed. (See the included LICENSE file.)

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <ALACBitUtilities.h>
#include <ALACEncoder.h>
#include <ALACDecoder.h>

using namespace v8;
using namespace node;

namespace alac {


static Persistent<String> frames_per_packet_symbol;
static Persistent<String> sample_rate_symbol;
static Persistent<String> channels_symbol;
static Persistent<String> bit_depth_symbol;
static Persistent<String> cookie_symbol;

// XXX: Stolen from convert-utility.
enum
{
    kTestFormatFlag_16BitSourceData    = 1,
    kTestFormatFlag_20BitSourceData    = 2,
    kTestFormatFlag_24BitSourceData    = 3,
    kTestFormatFlag_32BitSourceData    = 4
};

static Handle<Value>
throw_alac_error(int32_t ret)
{
  // XXX: The remaining values are unused in the codec itself.
  switch (ret) {
    case kALAC_ParamError:
      return ThrowException(Exception::Error(
          String::New("ALAC error: invalid parameter")));
    case kALAC_MemFullError:
      return ThrowException(Exception::Error(
          String::New("ALAC error: out of memory")));
    default:
      return ThrowException(Exception::Error(
          String::New("ALAC error: unknown error")));
  }
}


class Encoder : ObjectWrap
{
public:
  static void
  Initialize(Handle<Object> target)
  {
    Handle<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "encode", Encode);
    // XXX: Finish is a no-op, so don't bother.

    target->Set(String::NewSymbol("Encoder"), t->GetFunction());
  }

  virtual
  ~Encoder() {}

private:
  Encoder() : enc_() {}

  static Handle<Value>
  New(const Arguments &args)
  {
    HandleScope scope;

    Handle<Object> o = Handle<Object>::Cast(args[0]);
    Handle<Value> v;

    Encoder *e = new Encoder();

    // Fill input format structure.
    AudioFormatDescription &inf = e->inf_;

    inf.mFormatID = kALACFormatLinearPCM;
    inf.mFormatFlags = 0;  // XXX: These are pretty much ignored.
    inf.mFramesPerPacket = 1;
    inf.mReserved = 0;

    inf.mSampleRate = o->Get(sample_rate_symbol)->NumberValue();
    inf.mChannelsPerFrame = o->Get(channels_symbol)->Uint32Value();
    inf.mBitsPerChannel = o->Get(bit_depth_symbol)->Uint32Value();

    inf.mBytesPerPacket = inf.mBytesPerFrame = inf.mChannelsPerFrame * (inf.mBitsPerChannel >> 3);

    // Fill output format structure.
    AudioFormatDescription &outf = e->outf_;

    outf.mFormatID = kALACFormatAppleLossless;
    outf.mReserved = 0;

    // XXX: Zero because it's VBR.
    outf.mBytesPerPacket = 0;
    outf.mBytesPerFrame = 0;
    outf.mBitsPerChannel = 0;

    outf.mFramesPerPacket = o->Get(frames_per_packet_symbol)->Uint32Value();
    outf.mSampleRate = o->Get(sample_rate_symbol)->NumberValue();
    outf.mChannelsPerFrame = o->Get(channels_symbol)->Uint32Value();
    switch (o->Get(bit_depth_symbol)->Uint32Value()) {
      case 20: outf.mFormatFlags = kTestFormatFlag_20BitSourceData; break;
      case 24: outf.mFormatFlags = kTestFormatFlag_24BitSourceData; break;
      case 32: outf.mFormatFlags = kTestFormatFlag_32BitSourceData; break;
      default: outf.mFormatFlags = kTestFormatFlag_16BitSourceData; break;
    }

    // Init encoder.
    e->enc_.SetFrameSize(e->outf_.mFramesPerPacket);
    int32_t ret = e->enc_.InitializeEncoder(e->outf_);
    if (ret != ALAC_noErr) {
      delete e;
      return throw_alac_error(ret);
    }

    // Build cookie buffer.
    uint32_t cookieSize = e->enc_.GetMagicCookieSize(e->outf_.mChannelsPerFrame);
    char cookie[cookieSize];
    e->enc_.GetMagicCookie(cookie, &cookieSize);

    // Init self.
    e->Wrap(args.This());
    e->handle_->Set(cookie_symbol, Buffer::New(cookie, cookieSize)->handle_);
    return scope.Close(e->handle_);
  }

  static Handle<Value>
  Encode(const Arguments &args)
  {
    HandleScope scope;

    Encoder *e = ObjectWrap::Unwrap<Encoder>(args.This());

    unsigned char *in = (unsigned char *) Buffer::Data(args[0]);
    unsigned char *out = (unsigned char *) Buffer::Data(args[1]);
    int32_t size = (int32_t) Buffer::Length(args[0]);

    int32_t ret = e->enc_.Encode(e->inf_, e->outf_, in, out, &size);
    if (ret != ALAC_noErr)
      return throw_alac_error(ret);

    return scope.Close(Uint32::New(size));
  }

private:
  ALACEncoder enc_;
  AudioFormatDescription inf_;
  AudioFormatDescription outf_;
};


class Decoder : ObjectWrap
{
public:
  static void
  Initialize(Handle<Object> target)
  {
    Handle<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "decode", Decode);

    target->Set(String::NewSymbol("Decoder"), t->GetFunction());
  }

  virtual
  ~Decoder() {}

private:
  Decoder() : dec_() {}

  static Handle<Value>
  New(const Arguments &args)
  {
    HandleScope scope;

    Handle<Object> o = Handle<Object>::Cast(args[0]);
    Handle<Value> v;

    Decoder *d = new Decoder();

    // Fill parameters.
    v = o->Get(cookie_symbol);
    d->channels_= o->Get(channels_symbol)->Uint32Value();
    d->frames_ = o->Get(frames_per_packet_symbol)->Uint32Value();

    // Init decoder.
    int32_t ret = d->dec_.Init(Buffer::Data(v), Buffer::Length(v));
    if (ret != ALAC_noErr) {
      delete d;
      return throw_alac_error(ret);
    }

    // Init self.
    d->Wrap(args.This());
    return scope.Close(d->handle_);
  }

  static Handle<Value>
  Decode(const Arguments &args)
  {
    HandleScope scope;

    Decoder *d = ObjectWrap::Unwrap<Decoder>(args.This());

    BitBuffer in;
    BitBufferInit(&in, (uint8_t *) Buffer::Data(args[0]), Buffer::Length(args[0]));
    uint8_t *out = (uint8_t *) Buffer::Data(args[1]);

    uint32_t numFrames;
    int32_t ret = d->dec_.Decode(&in, out, d->frames_, d->channels_, &numFrames);
    if (ret != ALAC_noErr)
      return throw_alac_error(ret);

    return scope.Close(Uint32::New(numFrames));
  }

private:
  ALACDecoder dec_;
  uint32_t channels_;
  uint32_t frames_;
};


static void
Initialize(Handle<Object> target)
{
  HandleScope scope;

  frames_per_packet_symbol = NODE_PSYMBOL("framesPerPacket");
  sample_rate_symbol = NODE_PSYMBOL("sampleRate");
  channels_symbol = NODE_PSYMBOL("channels");
  bit_depth_symbol = NODE_PSYMBOL("bitDepth");
  cookie_symbol = NODE_PSYMBOL("cookie");

  NODE_DEFINE_CONSTANT(target, kALACDefaultFramesPerPacket);
  NODE_DEFINE_CONSTANT(target, kALACMaxEscapeHeaderBytes);

  Encoder::Initialize(target);
  Decoder::Initialize(target);
}


} // namespace alac

extern "C" void
init(Handle<Object> target)
{
  alac::Initialize(target);
}

NODE_MODULE(binding, init)
