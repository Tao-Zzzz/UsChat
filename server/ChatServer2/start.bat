@echo off
set E:\Library\c_lib\grpc\visualpro\third_party\protobuf\Debug\protoc.exe
    set GRPC_PLUGIN_PATH=E:\Library\c_lib\grpc\visualpro\Debug\grpc_cpp_plugin.exe
        set PROTO_FILE=message.proto

echo Generating gRPC code...
    %PROTOC_PATH% -I="." --grpc_out="." --plugin=protoc-gen-grpc="%GRPC_PLUGIN_PATH%" "%PROTO_FILE%"

    echo Generating C++ code...
    %PROTOC_PATH% --cpp_out=. "%PROTO_FILE%"

    echo Done.