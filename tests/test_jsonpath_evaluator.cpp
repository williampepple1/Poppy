#include <iostream>
#include <cassert>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <core/JsonPathEvaluator.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_jsonpath_evaluator..." << std::endl;

    const char* sampleJson = R"json({
        "store": {
            "book": [
                {
                    "category": "reference",
                    "author": "Nigel Rees",
                    "title": "Sayings of the Century",
                    "price": 8.95
                },
                {
                    "category": "fiction",
                    "author": "Evelyn Waugh",
                    "title": "Sword of Honour",
                    "price": 12.99
                },
                {
                    "category": "fiction",
                    "author": "Herman Melville",
                    "title": "Moby Dick",
                    "isbn": "0-553-21311-3",
                    "price": 8.99
                }
            ],
            "bicycle": {
                "color": "red",
                "price": 19.95,
                "inStock": true
            }
        },
        "expensive": 10
    })json";

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(sampleJson));
    assert(!doc.isNull());

    // 1. Root & basic property access
    QJsonValue expensive = JsonPathEvaluator::evaluate(doc, "$.expensive");
    assert(expensive.isDouble() && expensive.toDouble() == 10);

    QJsonValue bicycleColor = JsonPathEvaluator::evaluate(doc, "store.bicycle.color");
    assert(bicycleColor.isString() && bicycleColor.toString() == "red");

    QJsonValue inStock = JsonPathEvaluator::evaluate(doc, "$.store.bicycle.inStock");
    assert(inStock.isBool() && inStock.toBool() == true);

    // 2. Array index access
    QJsonValue firstTitle = JsonPathEvaluator::evaluate(doc, "$.store.book[0].title");
    assert(firstTitle.isString() && firstTitle.toString() == "Sayings of the Century");

    QJsonValue secondAuthor = JsonPathEvaluator::evaluate(doc, "store.book[1].author");
    assert(secondAuthor.isString() && secondAuthor.toString() == "Evelyn Waugh");

    QJsonValue secondPrice = JsonPathEvaluator::evaluate(doc, "store.book[1].price");
    assert(secondPrice.isDouble() && secondPrice.toDouble() == 12.99);

    // 3. Array wildcard & projection
    QJsonValue authors = JsonPathEvaluator::evaluate(doc, "$.store.book[*].author");
    assert(authors.isArray());
    QJsonArray authArr = authors.toArray();
    assert(authArr.size() == 3);
    assert(authArr[0].toString() == "Nigel Rees");
    assert(authArr[1].toString() == "Evelyn Waugh");
    assert(authArr[2].toString() == "Herman Melville");

    QJsonValue allBooks = JsonPathEvaluator::evaluate(doc, "store.book[*]");
    assert(allBooks.isArray() && allBooks.toArray().size() == 3);

    // 4. Missing / Invalid paths
    QJsonValue nonExistent = JsonPathEvaluator::evaluate(doc, "store.missing.key");
    assert(nonExistent.isUndefined());

    QJsonValue outOfBounds = JsonPathEvaluator::evaluate(doc, "store.book[99].title");
    assert(outOfBounds.isUndefined());

    // 5. String formatting evaluation
    QString strRes = JsonPathEvaluator::evaluateToString(doc, "store.bicycle.color");
    assert(strRes == "red");

    QString numRes = JsonPathEvaluator::evaluateToString(doc, "store.bicycle.price");
    assert(numRes == "19.95");

    QString arrStr = JsonPathEvaluator::evaluateToString(doc, "store.book[*].author");
    assert(arrStr.contains("Nigel Rees") && arrStr.contains("Evelyn Waugh"));

    // 6. Array root document
    const char* arrayJson = R"json([
        {"id": 1, "name": "Alpha"},
        {"id": 2, "name": "Beta"}
    ])json";
    QJsonDocument arrayDoc = QJsonDocument::fromJson(QByteArray(arrayJson));
    QJsonValue firstItem = JsonPathEvaluator::evaluate(arrayDoc, "$[0].name");
    assert(firstItem.isString() && firstItem.toString() == "Alpha");

    QJsonValue allNames = JsonPathEvaluator::evaluate(arrayDoc, "$[*].name");
    assert(allNames.isArray() && allNames.toArray().size() == 2);
    assert(allNames.toArray()[1].toString() == "Beta");

    std::cout << "All JsonPathEvaluator tests passed successfully!" << std::endl;
    return 0;
}
