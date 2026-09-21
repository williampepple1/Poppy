#include <iostream>
#include <cassert>
#include <core/BruParser.h>
#include <core/BruWriter.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_graphql..." << std::endl;

    QString bruGraphQL = R"(meta {
  name: Fetch User Details
  type: http
  seq: 1
}

post {
  url: https://api.example.com/graphql
  body: graphql
  auth: none
}

body:graphql {
  query GetUser($id: ID!) {
    user(id: $id) {
      id
      name
      email
    }
  }
}

body:graphql:vars {
  {
    "id": "101"
  }
}
)";

    RequestModel req = BruParser::parse(bruGraphQL);

    assert(req.name == "Fetch User Details");
    assert(req.method == HttpMethod::POST);
    assert(req.bodyType == BodyType::GraphQL);
    assert(req.graphqlQuery.contains("query GetUser($id: ID!)"));
    assert(req.graphqlVariables.contains("\"id\": \"101\""));

    // Serialize and parse back
    QString serialized = BruWriter::serialize(req);
    assert(serialized.contains("body: graphql"));
    assert(serialized.contains("body:graphql {"));
    assert(serialized.contains("body:graphql:vars {"));

    RequestModel roundtrip = BruParser::parse(serialized);
    assert(roundtrip.name == "Fetch User Details");
    assert(roundtrip.bodyType == BodyType::GraphQL);
    assert(roundtrip.graphqlQuery == req.graphqlQuery);
    assert(roundtrip.graphqlVariables == req.graphqlVariables);

    std::cout << "test_graphql passed successfully!" << std::endl;
    return 0;
}
