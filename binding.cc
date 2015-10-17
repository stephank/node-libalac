// ALAC.node, © 2014 Stéphan Kochen
// MIT-licensed. (See the included LICENSE file.)

#include <v8.h>
#include <node.h>
#include <nan.h>
#include <node_buffer.h>
#include <ALACBitUtilities.h>
#include <ALACEncoder.h>
#include <ALACDecoder.h>

using namespace v8;
using namespace node;

namespace alac {


static Nan::Persistent<String> frames_per_packet_symbol;
static Nan::Persistent<String> sample_rate_symbol;
static Nan::Persistent<String> channels_symbol;
static Nan::Persistent<String> bit_depth_symbol;
static Nan::Persistent<String> cookie_symbol;

// XXX: Stolen from convert-utility.
enum
{
    kTestFormatFlag_16BitSourceData    = 1,
    kTestFormatFlag_20BitSourceData    = 2,
    kTestFormatFlag_24BitSourceData    = 3,
    kTestFormatFlag_32BitSourceData    = 4
};

void
throw_alac_error(int32_t ret)
{
  // XXX: The remaining values are unused in the codec itself.
  switch (ret) {
    case kALAC_ParamError:
      Nan::ThrowError("ALAC error: invalid parameter");
    case kALAC_MemFullError:
      Nan::ThrowError("ALAC error: out of memory");
    default:
      Nan::ThrowError("ALAC error: unknown error");
  }
}


class Encoder : public Nan::ObjectWrap
{
public:
  static void
  Initialize(Handle<Object> target)
  {
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(t, "encode", Encode);
    // XXX: Finish is a no-op, so don't bother.

    Nan::Set(target, Nan::New<String>("Encoder").ToLocalChecked(), t->GetFunction());
  }

  virtual
  ~Encoder() {}

private:
  Encoder() : enc_() {}

  static void
  New(const Nan::FunctionCallbackInfo<v8::Value>& info)
  {
    Nan::HandleScope scope;

    Local<Object> o = Nan::To<Object>(info[0]).ToLocalChecked();

    Encoder *e = new Encoder();

    // Fill input format structure.
    AudioFormatDescription &inf = e->inf_;

    inf.mFormatID = kALACFormatLinearPCM;
    inf.mFormatFlags = 0;  // XXX: These are pretty much ignored.
    inf.mFramesPerPacket = 1;
    inf.mReserved = 0;

    inf.mSampleRate = o->Get(Nan::New<String>(sample_rate_symbol))->NumberValue();
    inf.mChannelsPerFrame = o->Get(Nan::New<String>(channels_symbol))->Uint32Value();
    inf.mBitsPerChannel = o->Get(Nan::New<String>(bit_depth_symbol))->Uint32Value();

    inf.mBytesPerPacket = inf.mBytesPerFrame = inf.mChannelsPerFrame * (inf.mBitsPerChannel >> 3);

    // Fill output format structure.
    AudioFormatDescription &outf = e->outf_;

    outf.mFormatID = kALACFormatAppleLossless;
    outf.mReserved = 0;

    // XXX: Zero because it's VBR.
    outf.mBytesPerPacket = 0;
    outf.mBytesPerFrame = 0;
    outf.mBitsPerChannel = 0;

    outf.mFramesPerPacket = o->Get(Nan::New<String>(frames_per_packet_symbol))->Uint32Value();
    outf.mSampleRate = o->Get(Nan::New<String>(sample_rate_symbol))->NumberValue();
    outf.mChannelsPerFrame = o->Get(Nan::New<String>(channels_symbol))->Uint32Value();
    switch (o->Get(Nan::New<String>(bit_depth_symbol))->Uint32Value()) {
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
      throw_alac_error(ret);
      return;
    }

    // Build cookie buffer.
    uint32_t cookieSize = e->enc_.GetMagicCookieSize(e->outf_.mChannelsPerFrame);
    char cookie[cookieSize];
    e->enc_.GetMagicCookie(cookie, &cookieSize);

    // Init self.
    e->Wrap(info.This());
    e->handle()->Set(Nan::New<String>(cookie_symbol), Nan::CopyBuffer(cookie, cookieSize).ToLocalChecked());
    info.GetReturnValue().Set(info.This());
  }

  static void
  Encode(const Nan::FunctionCallbackInfo<v8::Value>& info)
  {
    Nan::HandleScope scope;

    Encoder *e = Nan::ObjectWrap::Unwrap<Encoder>(info.This());

    unsigned char *in = (unsigned char *) Buffer::Data(info[0]);
    unsigned char *out = (unsigned char *) Buffer::Data(info[1]);
    int32_t size = (int32_t) Buffer::Length(info[0]);

    int32_t ret = e->enc_.Encode(e->inf_, e->outf_, in, out, &size);
    if (ret != ALAC_noErr) {
      throw_alac_error(ret);
      return;
    }

    info.GetReturnValue().Set(Nan::New<Uint32>(size));
  }

private:
  ALACEncoder enc_;
  AudioFormatDescription inf_;
  AudioFormatDescription outf_;
};


class Decoder : public Nan::ObjectWrap
{
public:
  static void
  Initialize(Handle<Object> target)
  {
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(t, "decode", Decode);

    Nan::Set(target, Nan::New<String>("Decoder").ToLocalChecked(), t->GetFunction());
  }

  virtual
  ~Decoder() {}

private:
  Decoder() : dec_() {}

  static void
  New(const Nan::FunctionCallbackInfo<v8::Value>& info)
  {
    Nan::HandleScope scope;

    Handle<Object> o = Handle<Object>::Cast(info[0]);
    Handle<Value> v;

    Decoder *d = new Decoder();

    // Fill parameters.
    v = o->Get(Nan::New<String>(cookie_symbol));
    d->channels_= o->Get(Nan::New<String>(channels_symbol))->Uint32Value();
    d->frames_ = o->Get(Nan::New<String>(frames_per_packet_symbol))->Uint32Value();

    // Init decoder.
    int32_t ret = d->dec_.Init(Buffer::Data(v), Buffer::Length(v));
    if (ret != ALAC_noErr) {
      delete d;
      throw_alac_error(ret);
      return;
    }

    // Init self.
    d->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static void
  Decode(const Nan::FunctionCallbackInfo<v8::Value>& info)
  {
    Nan::HandleScope scope;

    Decoder *d = Nan::ObjectWrap::Unwrap<Decoder>(info.This());

    BitBuffer in;
    BitBufferInit(&in, (uint8_t *) Buffer::Data(info[0]), Buffer::Length(info[0]));
    uint8_t *out = (uint8_t *) Buffer::Data(info[1]);

    uint32_t numFrames;
    int32_t ret = d->dec_.Decode(&in, out, d->frames_, d->channels_, &numFrames);
    if (ret != ALAC_noErr) {
      throw_alac_error(ret);
      return;
    }

    info.GetReturnValue().Set(Nan::New<Uint32>(numFrames));
  }

private:
  ALACDecoder dec_;
  uint32_t channels_;
  uint32_t frames_;
};


static void
Initialize(Handle<Object> target)
{
  Nan::HandleScope scope;

  frames_per_packet_symbol.Reset(Nan::New<String>("framesPerPacket").ToLocalChecked());
  sample_rate_symbol.Reset(Nan::New<String>("sampleRate").ToLocalChecked());
  channels_symbol.Reset(Nan::New<String>("channels").ToLocalChecked());
  bit_depth_symbol.Reset(Nan::New<String>("bitDepth").ToLocalChecked());
  cookie_symbol.Reset(Nan::New<String>("cookie").ToLocalChecked());

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
