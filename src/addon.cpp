#include <napi.h>
#include <functional>
#include <iostream>
#include <atomic>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Geolocation.h>

using namespace winrt;
using namespace Windows::Devices::Geolocation;

struct Sugar {
  Napi::ThreadSafeFunction _thread;
  Napi::FunctionReference _callback;
};

class SugarGeolocator : public Napi::ObjectWrap<SugarGeolocator> {
 public:
  SugarGeolocator(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<SugarGeolocator>(info) {
    this->geoRunning = false;
  }
  ~SugarGeolocator() {}

  inline bool assertListenerParam(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
      Napi::TypeError::New(env, "The parameter count is incorrect. Expected 2.")
          .ThrowAsJavaScriptException();
      return false;
    }
    if (!info[0].IsString()) {
      Napi::TypeError::New(env, "Expected a string for the first parameter")
          .ThrowAsJavaScriptException();
      return false;
    }
    if (!info[1].IsFunction()) {
      Napi::TypeError::New(env, "Expected a function for the first parameter")
          .ThrowAsJavaScriptException();
      return false;
    }
    return true;
  }
  inline auto removeAll(Napi::Env env, std::vector<Sugar *> *listeners) {
    return std::remove_if(listeners->begin(), listeners->end(),
                          [env](Sugar *it) {
                            it->_thread.Release();
                            it->_callback.Reset();
                            return true;
                          });
  }
  /** 添加事件监听器 */
  void addListener(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (!assertListenerParam(info)) {
      return;
    }
    // 获取监听事件
    Napi::String eventName = info[0].As<Napi::String>();
    std::string stdEventName = eventName.Utf8Value();
    // 获取回调函数
    Napi::Function callback = info[1].As<Napi::Function>();

    std::vector<Sugar *> *listeners;
    if (stdEventName == "position-change") {
      listeners = &positionListeners;
    } else if (stdEventName == "status-change") {
      listeners = &statusListeners;
    } else {
      Napi::TypeError::New(env, "The event name is not supported.")
          .ThrowAsJavaScriptException();
      return;
    }
    if (this->includeListener(listeners, &callback)) {
      Napi::TypeError::New(env, "This function has been registered.")
          .ThrowAsJavaScriptException();
    }
    Napi::ThreadSafeFunction _thread =
        Napi::ThreadSafeFunction::New(env, callback, "PositionChangeCallback",
                                      0,  // Unlimited queue size
                                      1  // Only 1 thread will use this function
        );

    Sugar *sugar = new Sugar;
    sugar->_thread = _thread;
    sugar->_callback = Napi::Persistent(callback);
    listeners->push_back(sugar);
  }
  void removeListener(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (!assertListenerParam(info)) {
      return;
    }
    // 获取监听事件
    Napi::String eventName = info[0].As<Napi::String>();
    std::string stdEventName = eventName.Utf8Value();
    // 获取回调函数
    Napi::Function callback = info[1].As<Napi::Function>();

    std::vector<Sugar *> *listeners;
    if (stdEventName == "position-change") {
      listeners = &positionListeners;
    } else if (stdEventName == "status-change") {
      listeners = &statusListeners;
    } else {
      Napi::TypeError::New(env, "The event name is not supported.")
          .ThrowAsJavaScriptException();
      return;
    }

    auto remove = std::remove_if(
        listeners->begin(), listeners->end(), [callback, env](Sugar *it) {
          bool flag = it->_callback.Value().StrictEquals(callback);
          if (flag) {
            it->_thread.Release();
            it->_callback.Reset();
          }
          return flag;
        });
    listeners->erase(remove, listeners->end());
  }
  void removeAllListener(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
      Napi::TypeError::New(env, "Expected a string for the first parameter")
          .ThrowAsJavaScriptException();
      return;
    }
    // 获取监听事件
    Napi::String eventName = info[0].As<Napi::String>();
    std::string stdEventName = eventName.Utf8Value();

    std::vector<Sugar *> *listeners;
    if (stdEventName == "position-change") {
      listeners = &positionListeners;
    } else if (stdEventName == "status-change") {
      listeners = &statusListeners;
    } else {
      Napi::TypeError::New(env, "The event name is not supported.")
          .ThrowAsJavaScriptException();
      return;
    }

    listeners->erase(removeAll(env, listeners));
  }

  /** 判断现有监听器中是否存在该回调函数 */
  bool includeListener(std::vector<Sugar *> *listeners,
                       Napi::Function *callback) {
    for (auto it = listeners->begin(); it != listeners->end(); it++) {
      if ((*it)->_callback.Value().StrictEquals(*callback)) {
        return true;
      }
    }
    return false;
  }
  /** 启动GEO监听 */
  Napi::Value startGeolocator(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (this->geoRunning) {
      Napi::TypeError::New(info.Env(), "The geo module is already running.")
          .ThrowAsJavaScriptException();
      return env.Null();
    }
    this->geoRunning = true;
    this->nodePromise = new Napi::Promise::Deferred(env);

    // 创建线程
    this->geoThread = std::thread(std::bind(&SugarGeolocator::run, this));
    // 分离线程
    this->geoThread.detach();
    return this->nodePromise->Promise();
  }

  Napi::Value stopGeolocator(const Napi::CallbackInfo &info) {
    if (this->geoRunning) {
      this->geoRunning = false;
      if (this->geoThread.joinable()) {
        this->geoThread.join();
      }
      removeAll(info.Env(), &this->positionListeners);
      removeAll(info.Env(), &this->statusListeners);
    }
    nodePromise->Resolve(Napi::String::New(info.Env(), "Geolocator is stoped"));
    return Napi::Boolean::New(info.Env(), !this->geoRunning);
  }

  void run() {
    /**
     * 初始化 Windows 运行时中（默认在多线程单元中）的线程。
     * 该调用还会初始化 COM。
     * @see
     * https://learn.microsoft.com/zh-cn/windows/uwp/cpp-and-winrt-apis/get-started
     */
    winrt::init_apartment();

    this->geolocator = std::make_shared<Geolocator>();
    /** 设置GEO的移动阈值，当出现该设定值的位置变化才触发  */
    geolocator->MovementThreshold(5.0);
    /** 当定位发送变动时触发 */
    geolocator->PositionChanged([this](Geolocator const &,
                                       PositionChangedEventArgs const &args) {
      Geoposition position = args.Position();
      double latitude = position.Coordinate().Point().Position().Latitude;
      double longitude = position.Coordinate().Point().Position().Longitude;
      for (auto it = positionListeners.begin(); it != positionListeners.end();
           it++) {
        (*it)->_thread.BlockingCall(
            [latitude, longitude](Napi::Env env, Napi::Function jsCallback) {
              Napi::Object result = Napi::Object::New(env);
              result.Set("latitude", Napi::Number::New(env, latitude));
              result.Set("longitude", Napi::Number::New(env, longitude));
              jsCallback.Call({result});
            });
      }
    });

    geolocator->StatusChanged(
        [this](Geolocator const &, StatusChangedEventArgs const &args) {
          PositionStatus status = args.Status();
          std::string statusStr;
          switch (status) {
            case PositionStatus::Ready:
              statusStr = "Ready";
              break;
            case PositionStatus::Initializing:
              statusStr = "Initializing";
              break;
            case PositionStatus::NoData:
              statusStr = "NoData";
              break;
            case PositionStatus::Disabled:
              statusStr = "Disabled";
              break;
            case PositionStatus::NotInitialized:
              statusStr = "NotInitialized";
              break;
            case PositionStatus::NotAvailable:
              statusStr = "NotAvailable";
              break;
            default:
              statusStr = "Unknown";
              break;
          }
          for (auto it = statusListeners.begin(); it != statusListeners.end();
               it++) {
            (*it)->_thread.BlockingCall(
                [statusStr](Napi::Env env, Napi::Function jsCallback) {
                  jsCallback.Call({Napi::String::New(env, statusStr)});
                });
          }
        });
  }

 private:
  Napi::Promise::Deferred *nodePromise = nullptr;
  /** geo执行线程 */
  std::thread geoThread;
  /** 模块线程运行标识 */
  std::atomic<bool> geoRunning;
  /** geo对象 */
  std::shared_ptr<Geolocator> geolocator = nullptr;

  std::vector<Sugar *> positionListeners;
  std::vector<Sugar *> statusListeners;
};

/**
 * 最终对js暴露封装
 */
Napi::Object SugarGeolocatorInit(Napi::Env env, Napi::Object exports) {
  Napi::Function constructor = Napi::ObjectWrap<SugarGeolocator>::DefineClass(
      env, "SugarGeolocator",
      {
          Napi::ObjectWrap<SugarGeolocator>::InstanceMethod(
              "startGeolocator", &SugarGeolocator::startGeolocator),
          Napi::ObjectWrap<SugarGeolocator>::InstanceMethod(
              "stopGeolocator", &SugarGeolocator::stopGeolocator),
          Napi::ObjectWrap<SugarGeolocator>::InstanceMethod(
              "addListener", &SugarGeolocator::addListener),
          Napi::ObjectWrap<SugarGeolocator>::InstanceMethod(
              "removeListener", &SugarGeolocator::removeListener),
          Napi::ObjectWrap<SugarGeolocator>::InstanceMethod(
              "removeAllListener", &SugarGeolocator::removeAllListener),
      });
  Napi::Persistent(constructor).SuppressDestruct();
  exports.Set("SugarGeolocator", constructor);
  return exports;
}

// 定义模块
NODE_API_MODULE(addon, SugarGeolocatorInit)
