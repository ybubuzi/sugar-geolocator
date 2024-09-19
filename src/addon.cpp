#include <napi.h>
#include <iostream>
#include <thread>
#include <sstream>
#include <winrt/Windows.Devices.Geolocation.h>
#include <winrt/Windows.Foundation.h>


using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Geolocation;

 
Napi::Value GetPosition(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  // winrt::init_apartment();
  Geolocator geolocator;
  auto promise = Napi::Promise::Deferred::New(env);
  auto feedOp{ [env, promise, geolocator](){
    auto positionAsync = geolocator.GetGeopositionAsync();
    try
    {
      Napi::Object result = Napi::Object::New(env);
      auto asyncResult = positionAsync.get(); // 阻塞直到获取完成
      BasicGeoposition position = asyncResult.Coordinate().Point().Position();
      // std::cout << "Geoposition retrieved. position: " << position.Latitude << "," << position.Longitude << std::endl;
      result.Set("latitude", position.Latitude);
      result.Set("longitude", position.Longitude);
      promise.Resolve(result);
      // 可以进一步处理 position
    }
    catch (const hresult_error &e)
    {
      std::ostringstream oss;
      oss << "Error: " << e.message().c_str();
      std::string error = oss.str();
      promise.Reject(Napi::String::New(promise.Env(), error));
    }
  }}; 
  feedOp();
  return promise.Promise();
}

 
Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  
  exports.Set(Napi::String::New(env, "GetPosition"), Napi::Function::New(env, GetPosition));
  return exports;
}

NODE_API_MODULE(addon, Init)
