#pragma once

#include <functional>
#include <core/RequestModel.h>
#include <core/ResponseModel.h>

namespace poppy::network {

using CompletionCallback = std::function<void(const core::ResponseModel&)>;
using ProgressCallback = std::function<void(qint64 bytesReceived, qint64 bytesTotal)>;

class INetworkEngine {
public:
    virtual ~INetworkEngine() = default;

    virtual void sendRequestAsync(
        const core::RequestModel& req,
        CompletionCallback onComplete,
        ProgressCallback onProgress = nullptr
    ) = 0;

    virtual void cancelAll() = 0;
};

} // namespace poppy::network
