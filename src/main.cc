#include <node.h>
#include <nan.h>
#include "./convert.h"
#include "./fetch.h"

using v8::FunctionTemplate;
using v8::Handle;
using v8::Object;
using v8::String;

void Init(Handle<Object> exports) {
  exports->Set(NanNew<String>("convert"),
    NanNew<FunctionTemplate>(Convert)->GetFunction());

  exports->Set(NanNew<String>("fetch"),
    NanNew<FunctionTemplate>(Fetch)->GetFunction());
}

NODE_MODULE(gm_native, Init);
