#include <Magick++.h>
#include <node_buffer.h>
#include <iostream>
#include <string.h>
#include "./convert.h"

#include "./rapidjson/reader.h"
#include "./rapidjson/document.h"
#include "./rapidjson/error/en.h"

using namespace v8;

static std::string ToString(Handle<Value> str) {
  return std::string(*NanUtf8String(str));
}

class ConvertWorker : public NanAsyncWorker {
public:
  ConvertWorker(std::string src, std::string operations, NanCallback *callback) : NanAsyncWorker(callback), srcFilename(src) {
    this->Initialize(operations);
  }

  ConvertWorker(Handle<Value> src, std::string operations, NanCallback *callback) : NanAsyncWorker(callback), srcBlob(node::Buffer::Data(src), node::Buffer::Length(src)) {
    // Create a Blob out of src buffer
    this->Initialize(operations);
  }

  void Initialize (std::string operations) {
    const char* json = operations.c_str();

    if (document.Parse(json).HasParseError()) {
      fprintf(stderr, "\nError: %s\n", GetParseError_En(document.GetParseError()));
      throw;
    } else {
      document.Parse(json);
    }
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

      rapidjson::Value::ConstMemberIterator imageSize = document.FindMember("size");
      if (imageSize != document.MemberEnd())
        image.resize(imageSize->value.GetString());

      rapidjson::Value::ConstMemberIterator imageQuality = document.FindMember("quality");
      if (imageQuality != document.MemberEnd())
        image.quality(imageQuality->value.GetUint());

      rapidjson::Value::ConstMemberIterator font = document.FindMember("font");
      if (font != document.MemberEnd()) {
        image.font(font->value["path"].GetString());
        image.fontPointsize(font->value["pointSize"].GetDouble());
        //image.fillColor(Magick::Color("rgb(53456, 35209, 30583)"));
        image.draw(Magick::DrawableText(
          font->value["xPos"].GetDouble(),
          font->value["yPos"].GetDouble(),
          font->value["value"].GetString()
        ));
      }

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
      NanNewBufferHandle((char *)blob.data(), blob.length())
    };

    callback->Call(2, argv);
  }
private:
  std::string srcFilename;
  Magick::Blob srcBlob;
  Magick::Image image;
  rapidjson::Document document;
};


// Asynchronous access to the `Estimate()` function
NAN_METHOD(Convert) {
  NanScope();

  NanCallback *callback = new NanCallback(args[2].As<Function>());
  Handle<Array> opts =args[1].As<Array>();

  if (args[0]->IsString()) {
    NanAsyncQueueWorker(new ConvertWorker(ToString(args[0]), ToString(opts), callback));
  } else {
    NanAsyncQueueWorker(new ConvertWorker(args[0], ToString(opts), callback));
  }

  NanReturnUndefined();
}
