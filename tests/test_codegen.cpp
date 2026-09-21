#include <iostream>
#include <cassert>
#include <core/codegen/CodeGenerator.h>
#include <core/RequestModel.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_codegen..." << std::endl;

    RequestModel req;
    req.name = "Create Resource";
    req.method = HttpMethod::POST;
    req.url = "https://api.example.com/v1/resources";
    req.headers.append(HttpHeader{.name = "Content-Type", .value = "application/json", .enabled = true});
    req.headers.append(HttpHeader{.name = "X-Api-Key", .value = "secret-key", .enabled = true});
    req.auth.type = AuthType::Bearer;
    req.auth.bearerToken = "jwt-access-token";
    req.bodyType = BodyType::Json;
    req.bodyContent = "{\"title\": \"My Resource\", \"count\": 42}";

    // 1. cURL
    QString curlCode = CodeGenerator::generate(TargetLanguage::Curl, req);
    assert(curlCode.contains("curl --request POST"));
    assert(curlCode.contains("https://api.example.com/v1/resources"));
    assert(curlCode.contains("-H \"Authorization: Bearer jwt-access-token\""));
    assert(curlCode.contains("-H \"Content-Type: application/json\""));
    assert(curlCode.contains("-d"));

    // 2. Python requests
    QString pyCode = CodeGenerator::generate(TargetLanguage::PythonRequests, req);
    assert(pyCode.contains("import requests"));
    assert(pyCode.contains("url = \"https://api.example.com/v1/resources\""));
    assert(pyCode.contains("\"Authorization\": \"Bearer jwt-access-token\""));
    assert(pyCode.contains("requests.request(\"POST\""));

    // 3. JavaScript fetch
    QString jsCode = CodeGenerator::generate(TargetLanguage::JavaScriptFetch, req);
    assert(jsCode.contains("fetch(\"https://api.example.com/v1/resources\""));
    assert(jsCode.contains("\"method\": \"POST\""));
    assert(jsCode.contains("\"Authorization\": \"Bearer jwt-access-token\""));

    // 4. JavaScript Axios
    QString axiosCode = CodeGenerator::generate(TargetLanguage::JavaScriptAxios, req);
    assert(axiosCode.contains("const axios = require('axios')"));
    assert(axiosCode.contains("method: 'post'"));
    assert(axiosCode.contains("url: 'https://api.example.com/v1/resources'"));

    // 5. Go net/http
    QString goCode = CodeGenerator::generate(TargetLanguage::GoHttp, req);
    assert(goCode.contains("package main"));
    assert(goCode.contains("http.NewRequest(\"POST\""));
    assert(goCode.contains("client.Do(req)"));

    // 6. C++ libcurl
    QString cppCode = CodeGenerator::generate(TargetLanguage::CppCurl, req);
    assert(cppCode.contains("#include <curl/curl.h>"));
    assert(cppCode.contains("curl_easy_init()"));
    assert(cppCode.contains("CURLOPT_CUSTOMREQUEST, \"POST\""));
    assert(cppCode.contains("curl_easy_perform(curl)"));

    std::cout << "test_codegen PASSED!" << std::endl;
    return 0;
}
