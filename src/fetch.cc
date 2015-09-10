#include <Magick++.h>
#include <node_buffer.h>
#include <iostream>
#include <string.h>
#include "./fetch.h"

using namespace v8;

static std::string ToString(Handle<Value> str) {
  return std::string(*NanUtf8String(str));
}

class FetchWorker : public NanAsyncWorker {
public:
  FetchWorker(std::string src, NanCallback *callback) : NanAsyncWorker(callback), srcFilename(src) {
    //this->Initialize(src);
  }

  FetchWorker(Handle<Value> src, NanCallback *callback) : NanAsyncWorker(callback), srcBlob(node::Buffer::Data(src), node::Buffer::Length(src)) {
    // Create a Blob out of src buffer
    //this->Initialize(src);
  }

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    Magick::InitializeMagick(NULL);

    try {
      if (!srcFilename.empty()) {
        image.read(srcFilename);
      } else {
        image.read(srcBlob);
      }

      std::string xySize = image.size();

    } catch (const std::exception &err) {
      SetErrorMessage(err.what());
    }
  }

  void HandleOKCallback () {
    NanScope();
    Magick::Blob blob;
    image.write(&blob);

    Local<Value> argv[] = {
      NanNull(),
      NanNewBufferHandle((char *)blob.data(), blob.length()),
      NanNew<String>(image.size())
    };

    callback->Call(3, argv);
  }
private:
  std::string srcFilename;
  Magick::Blob srcBlob;
  Magick::Image image;
  std::string xySize;
};

NAN_METHOD(Fetch) {
  NanScope();

  NanCallback *callback = new NanCallback(args[1].As<Function>());

  if (args[0]->IsString()) {
    NanAsyncQueueWorker(new FetchWorker(ToString(args[0]), callback));
  } else {
    NanAsyncQueueWorker(new FetchWorker(args[0], callback));
  }

  NanReturnUndefined();
}
