#include <iostream>
#include <cassert>
#include <network/GrpcClient.h>
#include <QJsonDocument>
#include <QJsonObject>

using namespace poppy::network;

int main() {
    std::cout << "Running test_grpc..." << std::endl;

    // 1. Test statusToString
    assert(GrpcClient::statusToString(0) == "OK");
    assert(GrpcClient::statusToString(1) == "CANCELLED");
    assert(GrpcClient::statusToString(3) == "INVALID_ARGUMENT");
    assert(GrpcClient::statusToString(5) == "NOT_FOUND");
    assert(GrpcClient::statusToString(14) == "UNAVAILABLE");
    assert(GrpcClient::statusToString(16) == "UNAUTHENTICATED");

    // 2. Test parseProto
    QString proto = R"(syntax = "proto3";
package userservice;

service UserApi {
  rpc GetUser (UserRequest) returns (UserReply);
  rpc StreamEvents (StreamRequest) returns (stream EventReply);
}

message UserRequest {
  string user_id = 1;
  int32 timeout = 2;
}

message UserReply {
  string name = 1;
  bool active = 2;
}
)";

    auto def = GrpcClient::parseProto(proto);
    assert(def.syntax == "proto3");
    assert(def.packageName == "userservice");
    assert(def.services.size() == 1);
    const auto& srv = def.services[0];
    assert(srv.name == "UserApi");
    assert(srv.fullName == "userservice.UserApi");
    assert(srv.methods.size() == 2);

    const auto& m1 = srv.methods[0];
    assert(m1.name == "GetUser");
    assert(m1.inputType == "UserRequest");
    assert(m1.outputType == "UserReply");
    assert(!m1.clientStreaming);
    assert(!m1.serverStreaming);

    const auto& m2 = srv.methods[1];
    assert(m2.name == "StreamEvents");
    assert(m2.inputType == "StreamRequest");
    assert(m2.outputType == "EventReply");
    assert(m2.serverStreaming);

    assert(def.messageFields.contains("UserRequest"));
    assert(def.messageFields["UserRequest"].size() == 2);

    // 3. Test generateSampleJsonForMessage
    QString sampleJson = GrpcClient::generateSampleJsonForMessage("UserRequest", def);
    assert(!sampleJson.isEmpty());
    QJsonDocument doc = QJsonDocument::fromJson(sampleJson.toUtf8());
    assert(doc.isObject());
    QJsonObject obj = doc.object();
    assert(obj.contains("user_id"));
    assert(obj.contains("timeout"));
    assert(obj.value("timeout").toInt() == 100);

    std::cout << "test_grpc passed successfully!" << std::endl;
    return 0;
}
