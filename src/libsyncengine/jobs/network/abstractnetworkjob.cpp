/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "abstractnetworkjob.h"

#include "network/proxy.h"
#include "jobs/network/networkjobsparams.h"
#include "jobs/syncjob.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/AutoPtr.h>
#include <Poco/SharedPtr.h>
#include <Poco/InflatingStream.h>
#include <Poco/Error.h>

#include <iostream> // std::ios, std::istream, std::cout, std::cerr
#include <functional>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPRequest.h>

#define BUF_SIZE 1024

#define MAX_TRIALS 5

namespace KDC {

std::string AbstractNetworkJob::_userAgent = std::string();
Poco::Net::Context::Ptr AbstractNetworkJob::_context = nullptr;
AbstractNetworkJob::TimeoutHelper AbstractNetworkJob::_timeoutHelper;

AbstractNetworkJob::AbstractNetworkJob() {
    if (_userAgent.empty()) {
        _userAgent = KDC::CommonUtility::userAgentString();
    }

    if (!_context) {
        for (int trials = 1; trials <= std::min(_trials, MAX_TRIALS); trials++) {
            try {
                _context =
                        new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
                _context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
            } catch (Poco::Exception const &e) {
                if (trials < _trials) {
                    LOG_INFO(_logger, "Error in Poco::Net::Context constructor: " << errorText(e) << ", retrying...");
                    continue;
                } else {
                    LOG_INFO(_logger, "Error in Poco::Net::Context constructor: " << errorText(e));
                    throw std::runtime_error(errorText(e).c_str());
                }
            } catch (std::exception &e) {
                if (trials < _trials) {
                    LOG_INFO(_logger, "Unknown error in Poco::Net::Context constructor: " << errorText(e) << ", retrying...");
                } else {
                    LOG_ERROR(_logger, "Unknown error in Poco::Net::Context constructor: " << errorText(e));
                    throw std::runtime_error(errorText(e).c_str());
                }
            }
        }
    }
}

AbstractNetworkJob::~AbstractNetworkJob() {
    clearSession();
}

bool AbstractNetworkJob::isManagedError(const ExitInfo exitInfo) noexcept {
    static const std::set managedExitCauses = {
            ExitCause::InvalidName, ExitCause::ApiErr,        ExitCause::FileTooBig, ExitCause::NotFound,
            ExitCause::FileLocked,  ExitCause::QuotaExceeded, ExitCause::FileExists, ExitCause::ShareLinkAlreadyExists,
            ExitCause::Http5xx};

    switch (exitInfo.code()) {
        case ExitCode::BackError:
            return managedExitCauses.contains(exitInfo.cause());
        case ExitCode::NetworkError:
            return exitInfo.cause() == ExitCause::NetworkTimeout;
        case ExitCode::InvalidSync:
            return exitInfo.cause() == ExitCause::SyncDirAccessError;
        case ExitCode::UpdateRequired:
            return true;
        default:
            return false;
    }
}

ExitInfo AbstractNetworkJob::runJob() noexcept {
    std::string url = getUrl();
    if (url.empty()) {
        LOG_WARN(_logger, "URL is not set");
        return ExitInfo();
    }

    assert(!_httpMethod.empty());

    Poco::URI uri;
    ExitInfo outputExitInfo = ExitCode::Ok;
    for (int trials = 1; trials <= std::min(_trials, MAX_TRIALS); trials++) {
        outputExitInfo = ExitCode::Ok;

        if (trials > 1) {
            Utility::msleep(500); // Sleep for 0.5s
        }

        uri = Poco::URI(url);

        createSession(uri);

        try {
            if (outputExitInfo = canRun(); !outputExitInfo) {
                return outputExitInfo;
            }
        } catch (Poco::Exception const &e) {
            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString() << " : " << errorText(e));
            outputExitInfo = ExitCode::NetworkError;
            break;
        }
        if (outputExitInfo = setData(); !outputExitInfo) { // Must be called before setQueryParameters
            LOG_WARN(_logger, "Job " << jobId() << " is cancelled " << outputExitInfo);
            break;
        }

        setQueryParameters(uri);

        // Send request
        auto sendChrono = std::chrono::steady_clock::now();
        try {
            outputExitInfo = sendRequest(uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in sendRequest " << jobId() << " : " << errorText(e));
            outputExitInfo = ExitCode::NetworkError;
        }

        if (!outputExitInfo) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString() << " aborted");
                outputExitInfo = ExitCode::Ok;
                break;
            }

            if (outputExitInfo.code() == ExitCode::NetworkError && outputExitInfo.cause() == ExitCause::SocketsDefuncted) {
                break;
            }

            if (trials < _trials) {
                LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString() << ", retrying...");
                continue;
            }

            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString());
            break;
        }

        // Receive response
        try {
            outputExitInfo = receiveResponse(uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in receiveResponse " << jobId() << " : " << errorText(e));
            outputExitInfo = ExitCode::NetworkError;
        }

        if (!outputExitInfo) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString() << " aborted");
                outputExitInfo = ExitCode::Ok;
                break;
            }

            // Attempt to detect network timeout
            auto errChrono = std::chrono::steady_clock::now();
            std::chrono::duration<double> requestDuration = errChrono - sendChrono;
            if (outputExitInfo.code() == ExitCode::NetworkError) {
                _timeoutHelper.add(requestDuration);
                if (_timeoutHelper.isTimeoutDetected()) {
                    LOG_WARN(_logger, "Network timeout detected - value=" << _timeoutHelper.value());
                    outputExitInfo.setCause(ExitCause::NetworkTimeout);
                }
            }

            if (trials < _trials) {
                LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString() << ", retrying...");
                continue;
            }

            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString());
            break;
        }

        if (outputExitInfo.code() == ExitCode::TokenRefreshed || outputExitInfo.code() == ExitCode::RateLimited) {
            _trials++; // Add one more chance
            continue;
        } else if (isManagedError(outputExitInfo)) {
            break;
        } else {
            outputExitInfo = ExitCode::Ok;
            break;
        }
    }

    return outputExitInfo;
}

bool AbstractNetworkJob::hasHttpError(std::string *errorCode /*= nullptr*/) const {
    if (_resHttp.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        if (errorCode) *errorCode = std::to_string(_resHttp.getStatus());
        return true;
    }
    return false;
}

bool AbstractNetworkJob::hasErrorApi() const {
    if (hasHttpError()) return true;
    if (_errorCode.empty()) return false;
    return true;
}

void AbstractNetworkJob::addRawHeader(const std::string &key, const std::string &value) {
    _rawHeaders.insert_or_assign(key, value);
}

void AbstractNetworkJob::abort() {
    LOG_DEBUG(_logger, "Aborting session for job " << jobId());

    SyncJob::abort();

    abortSession();
}

void AbstractNetworkJob::unzip(std::istream &is, std::stringstream &ss) {
    Poco::InflatingInputStream inflater(is, Poco::InflatingStreamBuf::STREAM_GZIP);
    while (is) {
        Poco::StreamCopier::copyStream(inflater, ss);
    }
}

void AbstractNetworkJob::createSession(const Poco::URI &uri) {
    const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);

    if (_session) {
        // Redirection case
        clearSession();
    }

    _session.reset(new Poco::Net::HTTPSClientSession(uri.getHost(), uri.getPort(), _context));

    if (_customTimeout) {
        _session->setTimeout(Poco::Timespan(_customTimeout, 0));
    }

    // Set proxy params
    if (Proxy::instance()->proxyConfig().type() == ProxyType::HTTP) {
        _session->setProxy(Proxy::instance()->proxyConfig().hostName(),
                           static_cast<Poco::UInt16>(Proxy::instance()->proxyConfig().port()));
        if (Proxy::instance()->proxyConfig().needsAuth()) {
            _session->setProxyCredentials(Proxy::instance()->proxyConfig().user(), Proxy::instance()->proxyConfig().token());
        }
    }
}

void AbstractNetworkJob::clearSession() {
    const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);

    if (_session) {
        try {
            if (_session->connected()) {
                _session->flushRequest();
                _session->reset();
            }
        } catch (Poco::Exception &e) {
            // Not an issue
            LOG_DEBUG(_logger, "Error in clearSession " << jobId() << " : " << errorText(e));
        }
    }
}

void AbstractNetworkJob::abortSession() {
    if (_session) {
        try {
            if (_session->connected()) {
                _session->abort();
            }
        } catch (Poco::Exception &e) {
            // Not an issue
            LOG_DEBUG(_logger, "Error in abortSession " << jobId() << " : " << errorText(e));
        }
    }
}

ExitInfo AbstractNetworkJob::sendRequest(const Poco::URI &uri) {
    std::string path(uri.getPathAndQuery());
    if (path.empty()) {
        path = "/";
    }

    LOG_DEBUG(_logger, "Sending " << _httpMethod << " request " << jobId() << " : " << uri.toString());

    Poco::Net::HTTPRequest req(_httpMethod, path, Poco::Net::HTTPMessage::HTTP_1_1);

    // Set headers
    req.set("User-Agent", _userAgent);
    req.setContentType(getContentType());
    for (const auto &header: _rawHeaders) {
        req.add(header.first, header.second);
    }

    if (!_data.empty()) {
        req.setContentLength(static_cast<std::streamsize>(_data.size()));
    }

    // Send request, retrieve an open stream
    std::vector<std::reference_wrapper<std::ostream>> stream;
    try {
        const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
        if (_session) {
            stream.push_back(_session->sendRequest(req));
            if (ioOrLogicalErrorOccurred(stream[0].get())) {
                return processSocketError("invalid send stream", jobId());
            }
        }
    } catch (Poco::Exception &e) {
        return processSocketError("sendRequest exception", jobId(), e);
    } catch (std::exception &e) {
        return processSocketError("sendRequest exception", jobId(), e);
    }

    // Send data
    std::string::const_iterator itBegin = _data.begin();
    while (itBegin != _data.end()) {
        const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
        if (isAborted()) {
            LOG_DEBUG(_logger, "Request " << jobId() << ": aborting HTTPS session");
            return ExitInfo();
        }

        std::string::const_iterator itEnd = (_data.end() - itBegin > BUF_SIZE ? itBegin + BUF_SIZE : _data.end());
        try {
            stream[0].get() << std::string(itBegin, itEnd);
            if (ioOrLogicalErrorOccurred(stream[0].get())) {
                return processSocketError("stream write error", jobId());
            }
        } catch (Poco::Exception &e) {
            return processSocketError("send data exception", jobId(), e);
        } catch (std::exception &e) {
            return processSocketError("send data exception", jobId(), e);
        }

        if (isProgressTracked()) {
            addProgress(itEnd - itBegin);
        }

        itBegin = itEnd;
    }

    return ExitCode::Ok;
}

ExitInfo AbstractNetworkJob::receiveResponse(const Poco::URI &uri) {
    std::vector<std::reference_wrapper<std::istream>> stream;
    try {
        const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
        if (_session) {
            stream.push_back(_session->receiveResponse(_resHttp));
            if (ioOrLogicalErrorOccurred(stream[0].get())) {
                return processSocketError("invalid receive stream", jobId());
            }
        }
    } catch (Poco::Exception &e) {
        return processSocketError("receiveResponse exception", jobId(), e);
    } catch (std::exception &e) {
        return processSocketError("receiveResponse exception", jobId(), e);
    }

    if (isAborted()) {
        LOG_DEBUG(_logger, "Request " << jobId() << " aborted");
        return ExitCode::Ok;
    }

    LOG_DEBUG(_logger,
              "Request " << jobId() << " finished with status: " << _resHttp.getStatus() << " / " << _resHttp.getReason());

    if (Utility::isError500(_resHttp.getStatus())) {
        std::string replyBody;
        getStringFromStream(stream[0].get(), replyBody);
        LOG_WARN(_logger, "Reply " << jobId() << ": " << replyBody);
        disableRetry();
        return {ExitCode::BackError, ExitCause::Http5xx};
    }

    switch (_resHttp.getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_OK: {
            try {
                const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
                return handleResponse(stream[0].get());
            } catch (std::exception &e) {
                LOG_WARN(_logger, "handleResponse exception: " << errorText(e));
                return ExitInfo();
            }
            break;
        }
        case Poco::Net::HTTPResponse::HTTP_FOUND: {
            // Redirection
            if (isAborted()) break;

            if (const auto exitInfo = followRedirect(); !exitInfo) {
                if (exitInfo.code() != ExitCode::Ok && exitInfo.code() != ExitCode::DataError) {
                    LOG_WARN(_logger, "Redirect handling failed");
                }
                return exitInfo;
            }
            return ExitCode::Ok;
        }
        case Poco::Net::HTTPResponse::HTTP_UNPROCESSABLE_ENTITY: {
            disableRetry();
            std::string replyBody;
            getStringFromStream(stream[0].get(), replyBody);
            LOG_WARN(_logger, "Reply " << jobId() << ": " << replyBody);
            (void) extractJsonError(replyBody);
            return {ExitCode::BackError, ExitCause::HttpErr};
        }
        case Poco::Net::HTTPResponse::HTTP_UPGRADE_REQUIRED: {
            LOG_WARN(_logger, "Received HTTP_UPGRADE_REQUIRED, update required");
            return ExitCode::UpdateRequired;
        }
        case Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS: {
            // Rate limitation
            LOG_WARN(_logger, "Received HTTP_TOO_MANY_REQUESTS, rate limited");
            return ExitCode::RateLimited;
        }
        default: {
            if (!isAborted()) {
                ExitInfo exitInfo;
                try {
                    exitInfo = handleError(stream[0].get(), uri);
                } catch (std::exception &e) {
                    LOG_WARN(_logger, "handleError failed: " << errorText(e));
                    return ExitInfo();
                }

                if (!exitInfo) {
                    if (exitInfo.code() != ExitCode::DataError &&
                        exitInfo.code() != ExitCode::InvalidToken &&
                        (exitInfo.code() != ExitCode::BackError || exitInfo.cause() != ExitCause::NotFound)) {
                        LOG_WARN(_logger, "Error handling failed");
                    }
                    return exitInfo;
                }
            }
            break;
        }
    }

    return ExitCode::Ok;
}

ExitInfo AbstractNetworkJob::handleError(std::istream &inputStream, const Poco::URI &uri) {
    std::string replyBody;
    getStringFromStream(inputStream, replyBody);
    LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << CommonUtility::s2ws(replyBody));
    return handleError(replyBody, uri);
}

void AbstractNetworkJob::getStringFromStream(std::istream &inputStream, std::string &res) {
    if (const std::string encoding = _resHttp.get("content-encoding", ""); encoding == "gzip") {
        std::stringstream ss;
        unzip(inputStream, ss);
        res = ss.str();
    } else {
        std::string tmp(std::istreambuf_iterator<char>(inputStream), (std::istreambuf_iterator<char>()));
        res = std::move(tmp);
    }
}

ExitInfo AbstractNetworkJob::followRedirect() {
    // Get redirection URL
    std::string redirectUrl;
    redirectUrl = _resHttp.get("Location", "");

    if (redirectUrl.empty()) {
        LOG_WARN(_logger, "Request " << jobId() << ": Failed to retrieve redirection URL");
        return {ExitCode::DataError, ExitCause::RedirectionError};
    }

    const Poco::URI uri(redirectUrl);

    // Follow redirection
    LOG_DEBUG(_logger, "Request " << jobId() << ", following redirection: " << redirectUrl);
    createSession(uri);

    if (const auto exitInfo = sendRequest(uri); !exitInfo) {
        return exitInfo;
    }
    return receiveResponse(uri);
}

ExitInfo AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId) {
    const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
    if (_session) {
        int err = _session->socket().getError();
        std::string errMsg = Poco::Error::getMessage(err);
        return processSocketError(msg, jobId, err, errMsg);
    } else {
        return processSocketError(msg, jobId, 0, std::string());
    }
}

ExitInfo AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, const std::exception &e) {
    return processSocketError(msg, jobId, 0, e.what());
}

ExitInfo AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, const Poco::Exception &e) {
    return processSocketError(msg, jobId, e.code(), e.message());
}

ExitInfo AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, int err,
                                                const std::string &errMsg) {
    if (isAborted()) {
        return ExitCode::Ok;
    }

    std::stringstream errMsgStream;
    errMsgStream << msg.c_str();
    if (jobId) errMsgStream << " - job " << jobId;
    if (err) errMsgStream << " : (" << err << ")";
    if (!errMsg.empty()) errMsgStream << " : " << errMsg.c_str();
    LOG_WARN(_logger, errMsgStream.str());

    ExitInfo exitInfo = ExitCode::NetworkError;
    if (err == EBADF) {
        // !!! macOS
        // When too many sockets are opened, the kernel kills all the process' sockets!
        // Console message generated: "mbuf_watchdog_defunct: defuncting all sockets from kDrive.<process id>"
        // macOS !!!
        LOG_WARN(_logger, "Sockets defuncted by kernel");
        exitInfo.setCause(ExitCause::SocketsDefuncted);
    } else {
        exitInfo.setCause(ExitCause::Unknown);
    }
    return exitInfo;
}

bool AbstractNetworkJob::ioOrLogicalErrorOccurred(std::ios &stream) {
    const bool res = (stream.fail() || stream.bad()) && !stream.eof();
    if (res) {
        LOG_DEBUG(_logger, "sendRequest failed for job " << jobId() << " - stream fail=" << stream.fail()
                                                         << " - stream bad=" << stream.bad() << " - stream eof=" << stream.eof());
    }
    return res;
}

std::string AbstractNetworkJob::errorText(const Poco::Exception &e) const {
    std::ostringstream error;
    error << e.className() << " : (" << e.code() << ") : " << e.displayText().c_str();
    return error.str();
}

std::string AbstractNetworkJob::errorText(const std::exception &e) const {
    std::ostringstream error;
    error << "(" << e.what() << ")";
    return error.str();
}

ExitInfo AbstractNetworkJob::handleJsonResponse(const std::string &replyBody) {
    return extractJson(replyBody, _jsonRes);
}

ExitInfo AbstractNetworkJob::handleOctetStreamResponse(std::istream &is) {
    getStringFromStream(is, _octetStreamRes);
    return ExitCode::Ok;
}

ExitInfo AbstractNetworkJob::extractJson(const std::string &replyBody, Poco::JSON::Object::Ptr &jsonObj) {
    try {
        jsonObj = Poco::JSON::Parser{}.parse(replyBody).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception &exc) {
        LOGW_WARN(_logger, L"Reply " << jobId() << L" received doesn't contain a valid JSON payload: "
                                     << CommonUtility::s2ws(exc.displayText()));
        Utility::logGenericServerError(_logger, "Request error", replyBody, _resHttp);
        return {ExitCode::BackError, ExitCause::ApiErr};
    }

    if (isExtendedLog()) {
        std::ostringstream os;
        jsonObj->stringify(os);
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << CommonUtility::s2ws(os.str()));
    }
    return ExitCode::Ok;
}

ExitInfo AbstractNetworkJob::extractJsonError(const std::string &replyBody, Poco::JSON::Object::Ptr errorObjPtr /*= nullptr*/) {
    Poco::JSON::Object::Ptr jsonObj;
    if (const auto exitInfo = extractJson(replyBody, jsonObj); !exitInfo) return exitInfo;

    errorObjPtr = jsonObj->getObject(errorKey);
    if (!JsonParserUtility::extractValue(errorObjPtr, codeKey, _errorCode)) {
        return ExitInfo();
    }
    if (!JsonParserUtility::extractValue(errorObjPtr, descriptionKey, _errorDescr)) {
        return ExitInfo();
    }

    return ExitCode::Ok;
}

void AbstractNetworkJob::TimeoutHelper::add(std::chrono::duration<double> duration) {
    unsigned int roundDuration = static_cast<unsigned int>(round(duration.count() / PRECISION) * PRECISION);
    if (roundDuration >= _maxDuration) {
        LOG_DEBUG(Log::instance()->getLogger(), "TimeoutHelper - Timeout detected value=" << roundDuration);
        if (roundDuration > _maxDuration) {
            LOG_DEBUG(Log::instance()->getLogger(), "TimeoutHelper - New timeout threshold");
            _maxDuration = roundDuration;
            clearAllEvents();
        }

        // Add event
        const std::scoped_lock<std::mutex> lock(_mutexEventsQueue);
        unsigned int eventTime = static_cast<unsigned int>(time(NULL));
        _eventsQueue.push(eventTime);
    }
}

void AbstractNetworkJob::TimeoutHelper::clearAllEvents() {
    const std::scoped_lock<std::mutex> lock(_mutexEventsQueue);
    std::queue<SyncTime> emptyQueue;
    std::swap(_eventsQueue, emptyQueue);
}

void AbstractNetworkJob::TimeoutHelper::deleteOldestEvents() {
    const std::scoped_lock<std::mutex> lock(_mutexEventsQueue);
    auto eventTime = static_cast<unsigned int>(time(NULL));
    while (!_eventsQueue.empty() && eventTime - _eventsQueue.front() > PERIOD) {
        LOG_DEBUG(Log::instance()->getLogger(), "TimeoutHelper - Clear event " << _eventsQueue.front());
        _eventsQueue.pop();
    }
}

unsigned int AbstractNetworkJob::TimeoutHelper::count() {
    deleteOldestEvents();
    return static_cast<unsigned int>(_eventsQueue.size());
}

} // namespace KDC
