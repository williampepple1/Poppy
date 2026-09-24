#include <iostream>
#include <cassert>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <core/RequestModel.h>
#include <core/exporters/PostmanExporter.h>
#include <core/exporters/InsomniaExporter.h>
#include <core/exporters/HarExporter.h>
#include <core/importers/InsomniaImporter.h>
#include <core/CsvParser.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_exporters..." << std::endl;

    QList<RequestModel> requests;

    RequestModel req1;
    req1.name = "List Items";
    req1.method = HttpMethod::GET;
    req1.url = "https://api.example.com/items?limit=10";
    req1.headers.append(HttpHeader{"Accept", "application/json", true});
    requests.append(req1);

    RequestModel req2;
    req2.name = "Create Item";
    req2.method = HttpMethod::POST;
    req2.url = "https://api.example.com/items";
    req2.bodyType = BodyType::Json;
    req2.bodyContent = "{\"title\": \"Widget\"}";
    requests.append(req2);

    // 1. Postman Exporter
    {
        QJsonObject postman = PostmanExporter::exportToJson(requests, "Test Collection");
        assert(postman.contains("info"));
        QJsonObject info = postman.value("info").toObject();
        assert(info.value("name").toString() == "Test Collection");
        assert(info.value("schema").toString().contains("schema.getpostman.com/json/collection/v2.1.0"));

        assert(postman.contains("item"));
        QJsonArray items = postman.value("item").toArray();
        assert(items.size() == 2);
        assert(items[0].toObject().value("name").toString() == "List Items");
        assert(items[1].toObject().value("name").toString() == "Create Item");

        QJsonObject reqObj = items[0].toObject().value("request").toObject();
        assert(reqObj.value("method").toString() == "GET");
    }

    // 2. Insomnia Exporter
    {
        QJsonObject insomnia = InsomniaExporter::exportToJson(requests, "Test Workspace");
        assert(insomnia.value("_type").toString() == "export");
        assert(insomnia.value("__export_format").toInt() == 4);
        assert(insomnia.contains("resources"));
        QJsonArray resources = insomnia.value("resources").toArray();
        assert(resources.size() >= 3); // workspace + 2 requests
        bool foundGet = false;
        for (const auto& rVal : resources) {
            QJsonObject r = rVal.toObject();
            if (r.value("_type").toString() == "request" && r.value("name").toString() == "List Items") {
                foundGet = true;
                assert(r.value("method").toString() == "GET");
            }
        }
        assert(foundGet);
    }

    // 3. HAR Exporter
    {
        QJsonObject har = HarExporter::exportToJson(requests);
        assert(har.contains("log"));
        QJsonObject log = har.value("log").toObject();
        assert(log.value("version").toString() == "1.2");
        assert(log.contains("creator"));
        assert(log.contains("entries"));
        QJsonArray entries = log.value("entries").toArray();
        assert(entries.size() == 2);
        QJsonObject firstEntry = entries[0].toObject();
        assert(firstEntry.contains("request"));
        QJsonObject r = firstEntry.value("request").toObject();
        assert(r.value("method").toString() == "GET");
        assert(r.value("url").toString() == "https://api.example.com/items?limit=10");

        RequestModel filtered;
        filtered.method = HttpMethod::GET;
        filtered.url = "https://api.example.com/items";
        filtered.queryParams.append(HttpParam{.key = "shown", .value = "2", .enabled = true});
        filtered.queryParams.append(HttpParam{.key = "hidden", .value = "1", .enabled = false});
        QJsonObject filteredHar = HarExporter::exportToJson({filtered});
        QJsonObject filteredReq = filteredHar.value("log").toObject().value("entries").toArray().first().toObject().value("request").toObject();
        assert(filteredReq.value("url").toString().contains("shown=2"));
        assert(!filteredReq.value("url").toString().contains("hidden"));
        bool sawHidden = false;
        bool sawShown = false;
        for (const auto& qv : filteredReq.value("queryString").toArray()) {
            const QString name = qv.toObject().value("name").toString();
            if (name == "hidden") sawHidden = true;
            if (name == "shown") sawShown = true;
        }
        assert(sawShown);
        assert(!sawHidden);
    }

    {
        RequestModel gql;
        gql.name = "Ping";
        gql.method = HttpMethod::POST;
        gql.url = "https://api.example.com/graphql";
        gql.bodyType = BodyType::GraphQL;
        gql.graphqlQuery = "query { ping }";
        gql.graphqlVariables = "{\"id\":1}";
        QJsonObject insomnia = InsomniaExporter::exportToJson({gql}, "Gql");
        QString bodyText;
        for (const auto& resource : insomnia.value("resources").toArray()) {
            QJsonObject obj = resource.toObject();
            if (obj.value("name").toString() == "Ping") {
                bodyText = obj.value("body").toObject().value("text").toString();
            }
        }
        QJsonObject gqlBody = QJsonDocument::fromJson(bodyText.toUtf8()).object();
        assert(gqlBody.value("variables").isObject());
        assert(gqlBody.value("variables").toObject().value("id").toInt() == 1);
        RequestModel imported = InsomniaImporter::parseInsomniaRequest(QJsonObject{
            {"body", QJsonObject{{"mimeType", "application/graphql"}, {"text", bodyText}}}
        });
        assert(imported.graphqlVariables.contains("\"id\""));
    }

    {
        const auto rows = parseCsvTable("name,email\n\"Alice\",\"bob@test.com, Inc\"\n");
        assert(rows.size() == 1);
        assert(rows[0].value("name") == "Alice");
        assert(rows[0].value("email") == "bob@test.com, Inc");
    }

    std::cout << "test_exporters passed successfully!" << std::endl;
    return 0;
}
