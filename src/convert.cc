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

static Magick::GravityType ToGravityType(std::string gravity) {

  printf("%s\n", gravity.c_str());

  if (gravity == "Center")         return Magick::CenterGravity;
  else if (gravity == "East")      return Magick::EastGravity;
  else if (gravity == "Forget")    return Magick::ForgetGravity;
  else if (gravity == "NorthEast") return Magick::NorthEastGravity;
  else if (gravity == "North")     return Magick::NorthGravity;
  else if (gravity == "NorthWest") return Magick::NorthWestGravity;
  else if (gravity == "SouthEast") return Magick::SouthEastGravity;
  else if (gravity == "South")     return Magick::SouthGravity;
  else if (gravity == "SouthWest") return Magick::SouthWestGravity;
  else if (gravity == "West")      return Magick::WestGravity;
  else {
    return Magick::ForgetGravity;
  }
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
    Magick::GravityType gravity;
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

      rapidjson::Value::ConstMemberIterator imageGravity = document.FindMember("gravity");
      if (imageGravity != document.MemberEnd()) {
        gravity = ToGravityType(imageGravity->value.GetString());
        image.draw(Magick::DrawableGravity(gravity));
      }

      if (document.HasMember("imageOverlays")) {
        const rapidjson::Value& imageOverlays = document["imageOverlays"];
        for (rapidjson::SizeType i = 0; i < imageOverlays.Size(); i++) {

          if (imageOverlays[i]["cover"].GetBool()) {
            image.draw(Magick::DrawableCompositeImage(
              imageOverlays[i]["xPos"].GetDouble(),
              imageOverlays[i]["yPos"].GetDouble(),
              document["imgResWidth"].GetDouble(),
              document["imgResHeight"].GetDouble(),
              imageOverlays[i]["path"].GetString()
            ));
          } else {
            image.draw(Magick::DrawableCompositeImage(
              imageOverlays[i]["xPos"].GetDouble(),
              imageOverlays[i]["yPos"].GetDouble(),
              imageOverlays[i]["path"].GetString()
            ));
          }
        }
      }

      if (document.HasMember("textOverlays")) {
        const rapidjson::Value& textOverlays = document["textOverlays"];
        for (rapidjson::SizeType i = 0; i < textOverlays.Size(); i++) {

          std::string text = textOverlays[i]["value"].GetString();
          std::string printedText;
          if (textOverlays[i].HasMember("trimLength")) {
            double trimLength = textOverlays[i]["trimLength"].GetDouble();
            printedText = text.length() > trimLength ?
                                        text.substr (0,trimLength) + "..." :
                                        text;
          } else {
            printedText = text;
          }

          image.font(textOverlays[i]["path"].GetString());
          image.fontPointsize(textOverlays[i]["pointSize"].GetDouble());
          image.fillColor(Magick::ColorRGB("rgb(255, 255, 255)"));
          image.draw(Magick::DrawableText(
            textOverlays[i]["xPos"].GetDouble(),
            textOverlays[i]["yPos"].GetDouble(),
            printedText
          ));

        }
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

NAN_METHOD(Convert) {
  NanScope();

  NanCallback *callback = new NanCallback(args[2].As<Function>());

  if (args[0]->IsString()) {
    NanAsyncQueueWorker(new ConvertWorker(ToString(args[0]), ToString(args[1]), callback));
  } else {
    NanAsyncQueueWorker(new ConvertWorker(args[0], ToString(args[1]), callback));
  }

  NanReturnUndefined();
}
