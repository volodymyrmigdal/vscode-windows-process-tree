/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include <nan.h>
#include "cpu_worker.h"
#include "process_worker.h"

void GetProcessList(const Nan::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 3) {
    Nan::ThrowTypeError("GetProcessList expects three arguments.");
    return;
  }


  if (!args[0]->IsNumber()) {
    Nan::ThrowTypeError("The first argument of GetProcessList, rootPid, must be a number.");
    return;
  }

  if (!args[1]->IsFunction()) {
    Nan::ThrowTypeError("The second argument of GetProcessList, callback, must be a function.");
    return;
  }

  if (!args[2]->IsNumber()) {
    Nan::ThrowTypeError("The third argument of GetProcessList, flags, must be a number.");
    return;
  }

  DWORD* rootPid = new DWORD;
  *rootPid = (DWORD)(Nan::To<int32_t>(args[0]).FromJust());
  Nan::Callback *callback = new Nan::Callback(v8::Local<v8::Function>::Cast(args[1]));
  DWORD* flags = new DWORD;
  *flags = (DWORD)(Nan::To<int32_t>(args[2]).FromJust());
  GetProcessesWorker *worker = new GetProcessesWorker( rootPid, callback, flags);
  Nan::AsyncQueueWorker(worker);
}

void GetProcessCpuUsage(const Nan::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 2) {
      Nan::ThrowTypeError("GetProcessCpuUsage expects two arguments.");
      return;
  }

  if (!args[0]->IsArray()) {
    Nan::ThrowTypeError("The first argument of GetProcessCpuUsage, callback, must be an array.");
    return;
  }

  if (!args[1]->IsFunction()) {
    Nan::ThrowTypeError("The second argument of GetProcessCpuUsage, flags, must be a function.");
    return;
  }

  // Read the ProcessTreeNode JS object
  v8::Local<v8::Array> processes = v8::Local<v8::Array>::Cast(args[0]);
  Nan::Callback *callback = new Nan::Callback(v8::Local<v8::Function>::Cast(args[1]));
  GetCPUWorker *worker = new GetCPUWorker(callback, processes);
  Nan::AsyncQueueWorker(worker);
}

void GetProcessCreationTime(const Nan::FunctionCallbackInfo<v8::Value>& args) {

  v8::Isolate* isolate = args.GetIsolate();

  if (args.Length() < 1) {
    Nan::ThrowTypeError("GetProcessCreationTime expects single argument.");
    return;
  }

  if (!args[0]->IsNumber()) {
    Nan::ThrowTypeError("The first argument of GetProcessCreationTime, pid, must be a number.");
    return;
  }

  DWORD pid =(DWORD)(Nan::To<int32_t>(args[0]).FromJust());
  bool err = false;
  ULONGLONG creationTime = processCreationTimeGet( pid, err );

  if( err ) {
    Nan::ThrowError( Nan::ErrnoException( errno, NULL, "Failed to get process creation time. Error code:" ) );
  }
  else if( creationTime ) {
    auto result = v8::BigInt::NewFromUnsigned( isolate, creationTime );
    args.GetReturnValue().Set( result );
  }
  else {
    args.GetReturnValue().Set( v8::Null( isolate ) );
  }

}

void Init(v8::Local<v8::Object> exports) {
  Nan::Export(exports, "getProcessList", GetProcessList);
  Nan::Export(exports, "getProcessCpuUsage", GetProcessCpuUsage);
  Nan::Export(exports, "getProcessCreationTime", GetProcessCreationTime);
}

NODE_MODULE(hello, Init)
