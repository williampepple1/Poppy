#pragma once

#include <QString>
#include <QList>
#include <core/RequestModel.h>

namespace poppy::core {

class AwsSigV4Signer {
public:
    static QList<HttpHeader> generateAuthHeaders(const RequestModel& req);
    static QString computeSignature(const QString& secretKey, const QString& dateStamp, 
                                    const QString& region, const QString& service, 
                                    const QString& stringToSign);
};

} // namespace poppy::core
