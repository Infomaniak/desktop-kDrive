/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"

#include <log4cplus/loggingmacros.h>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/AutoPtr.h>
#include <Poco/SAX/InputSource.h>
#include <Poco/SharedPtr.h>
#include <Poco/InflatingStream.h>
#include <Poco/Error.h>

#include <iostream> // std::ios, std::istream, std::cout, std::cerr
#include <functional>
#include <Poco/JSON/Parser.h>

#define ABSTRACTNETWORKJOB_NEW_ERROR_MSG "Failed to create AbstractNetworkJob instance!"

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
                    LOG_INFO(_logger, "Error in Poco::Net::Context constructor: " << errorText(e).c_str() << ", retrying...");
                    continue;
                } else {
                    LOG_INFO(_logger, "Error in Poco::Net::Context constructor: " << errorText(e).c_str());
                    throw std::runtime_error(ABSTRACTNETWORKJOB_NEW_ERROR_MSG);
                    break;
                }
            } catch (std::exception &e) {
                if (trials < _trials) {
                    LOG_INFO(_logger,
                             "Unknown error in Poco::Net::Context constructor: " << errorText(e).c_str() << ", retrying...");
                } else {
                    LOG_ERROR(_logger, "Unknown error in Poco::Net::Context constructor: " << errorText(e).c_str());
                    throw std::runtime_error(ABSTRACTNETWORKJOB_NEW_ERROR_MSG);
                }
            }
        }
    }
}

AbstractNetworkJob::~AbstractNetworkJob() {
    clearSession();
}

bool AbstractNetworkJob::isManagedError(ExitCode exitCode, ExitCause exitCause) noexcept {
    static const std::set<ExitCause> managedExitCauses = {ExitCause::InvalidName,   ExitCause::ApiErr,
                                                          ExitCause::FileTooBig,    ExitCause::NotFound,
                                                          ExitCause::QuotaExceeded, ExitCause::FileAlreadyExist};

    switch (exitCode) {
        case ExitCode::BackError:
            return managedExitCauses.find(exitCause) != managedExitCauses.cend();
        case ExitCode::NetworkError:
            return exitCause == ExitCause::NetworkTimeout;
        case ExitCode::UpdateRequired:
            return true;
        default:
            return false;
    }
}

void AbstractNetworkJob::runJob() noexcept {
    std::string url = getUrl();
    if (url == "") {
        LOG_WARN(_logger, "URL is not set");
        return;
    }

    assert(!_httpMethod.empty());

    Poco::URI uri;
    for (int trials = 1; trials <= std::min(_trials, MAX_TRIALS); trials++) {
        if (trials > 1) {
            Utility::msleep(500); // Sleep for 0.5s
        }

        uri = Poco::URI(url);

        createSession(uri);

        try {
            if (!canRun()) {
                return;
            }
        } catch (Poco::Exception const &e) {
            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str() << " : " << errorText(e).c_str());
            _exitCode = ExitCode::NetworkError;
            break;
        }

        bool canceled = false;
        setData(canceled); // Must be called before setQueryParameters
        if (canceled) {
            LOG_WARN(_logger, "Job " << jobId() << " is cancelled");
            _exitCode = ExitCode::DataError;
            break;
        }

        setQueryParameters(uri, canceled);
        if (canceled) {
            LOG_WARN(_logger, "Job " << jobId() << " is cancelled");
            _exitCode = ExitCode::DataError;
            break;
        }

        // Send request
        auto sendChrono = std::chrono::steady_clock::now();
        bool ret = false;
        try {
            ret = sendRequest(uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in sendRequest " << jobId() << " : " << errorText(e).c_str());
            _exitCode = ExitCode::NetworkError;
            ret = false;
        }

        if (!ret) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString().c_str() << " aborted");
                _exitCode = ExitCode::Ok;
                break;
            }

            if (_exitCode == ExitCode::NetworkError && _exitCause == ExitCause::SocketsDefuncted) {
                break;
            }

            if (trials < _trials) {
                LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str() << ", retrying...");
                continue;
            }

            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str());
            break;
        }

        // Receive response
        try {
            ret = receiveResponse(uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in receiveResponse " << jobId() << " : " << errorText(e).c_str());
            _exitCode = ExitCode::NetworkError;
            ret = false;
        }

        if (!ret) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString().c_str() << " aborted");
                _exitCode = ExitCode::Ok;
                break;
            }

            // Attempt to detect network timeout
            auto errChrono = std::chrono::steady_clock::now();
            std::chrono::duration<double> requestDuration = errChrono - sendChrono;
            if (_exitCode == ExitCode::NetworkError) {
                _timeoutHelper.add(requestDuration);
                if (_timeoutHelper.isTimeoutDetected()) {
                    LOG_WARN(_logger, "Network timeout detected - value=" << _timeoutHelper.value());
                    _exitCause = ExitCause::NetworkTimeout;
                }
            }

            if (trials < _trials) {
                LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str() << ", retrying...");
                continue;
            }

            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str());
            break;
        }

        if (_exitCode == ExitCode::TokenRefreshed || _exitCode == ExitCode::RateLimited) {
            _exitCode = ExitCode::Ok;
            _trials++; // Add one more chance
            continue;
        } else if (isManagedError(_exitCode, _exitCause)) {
            break;
        } else {
            _exitCode = ExitCode::Ok;
            break;
        }
    }
}

bool AbstractNetworkJob::hasHttpError() const {
    return _resHttp.getStatus() != Poco::Net::HTTPResponse::HTTP_OK;
}

bool AbstractNetworkJob::hasErrorApi(std::string *errorCode, std::string *errorDescr) const {
    if (hasHttpError()) return true;
    if (_errorCode.empty()) return false;
    if (errorCode) {
        *errorCode = _errorCode;
        if (errorDescr) {
            *errorDescr = _errorDescr;
        }
    }
    return true;
}

void AbstractNetworkJob::addRawHeader(const std::string &key, const std::string &value) {
    _rawHeaders.insert_or_assign(key, value);
}

void AbstractNetworkJob::abort() {
    LOG_DEBUG(_logger, "Aborting session for job " << jobId());

    AbstractJob::abort();

    abortSession();
}

void AbstractNetworkJob::unzip(std::istream &is, std::stringstream &ss) {
    Poco::InflatingInputStream inflater(is, Poco::InflatingStreamBuf::STREAM_GZIP);
    while (is) {
        Poco::StreamCopier::copyStream(inflater, ss);
    }
}

void AbstractNetworkJob::getStringFromStream(std::istream &is, std::string &res) {
    std::string tmp(std::istreambuf_iterator<char>(is), (std::istreambuf_iterator<char>()));
    res = std::move(tmp);
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
        _session->setProxy(Proxy::instance()->proxyConfig().hostName(), Proxy::instance()->proxyConfig().port());
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
            LOG_DEBUG(_logger, "Error in clearSession " << jobId() << " : " << errorText(e).c_str());
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
            LOG_DEBUG(_logger, "Error in abortSession " << jobId() << " : " << errorText(e).c_str());
        }
    }
}

bool AbstractNetworkJob::sendRequest(const Poco::URI &uri) {
    std::string path(uri.getPathAndQuery());
    if (path.empty()) {
        path = "/";
    }

    LOG_DEBUG(_logger, "Sending " << _httpMethod.c_str() << " request " << jobId() << " : " << uri.toString().c_str());

    // Get Content Type
    bool canceled = false;
    std::string contentType = getContentType(canceled);
    if (canceled) {
        LOG_WARN(_logger, "Unable to get content type!");
        _exitCode = ExitCode::DataError;
        return false;
    }

    Poco::Net::HTTPRequest req(_httpMethod, path, Poco::Net::HTTPMessage::HTTP_1_1);

    // Set headers
    req.set("User-Agent", _userAgent);
    req.setContentType(contentType);
    for (const auto &header: _rawHeaders) {
        req.add(header.first, header.second);
    }

    if (!_data.empty()) {
        req.setContentLength(_data.size());
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
            return false;
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

    return true;
}

bool AbstractNetworkJob::receiveResponse(const Poco::URI &uri) {
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
        return true;
    }

    LOG_DEBUG(_logger, "Request " << jobId() << " finished with status: " << _resHttp.getStatus() << " / "
                                  << _resHttp.getReason().c_str());

    bool res = true;
    switch (_resHttp.getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_OK: {
            bool ok = false;
            try {
                const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
                ok = handleResponse(stream[0].get());
            } catch (std::exception &e) {
                LOG_WARN(_logger, "handleResponse exception: " << errorText(e).c_str());
                return false;
            }

            if (!ok) {
                LOG_WARN(_logger, "Response handling failed");
                res = false;
            }
            break;
        }
        case Poco::Net::HTTPResponse::HTTP_FOUND: {
            // Redirection
            if (!isAborted()) {
                if (!followRedirect(stream[0].get())) {
                    if (_exitCode != ExitCode::Ok && _exitCode != ExitCode::DataError) {
                        LOG_WARN(_logger, "Redirect handling failed");
                    }
                    return false;
                }
                return true;
            }
            break;
        }
        case Poco::Net::HTTPResponse::HTTP_UPGRADE_REQUIRED: {
            _exitCode = ExitCode::UpdateRequired;
            LOG_WARN(_logger, "Received HTTP_UPGRADE_REQUIRED, update required");
            break;
        }
        case Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS: {
            // Rate limitation
            _exitCode = ExitCode::RateLimited;
            LOG_WARN(_logger, "Received HTTP_TOO_MANY_REQUESTS, rate limited");
        }
        default: {
            if (!isAborted()) {
                bool ok = false;
                try {
                    ok = handleError(stream[0].get(), uri);
                } catch (std::exception &e) {
                    LOG_WARN(_logger, "handleError failed: " << errorText(e).c_str());
                    return false;
                }

                if (!ok) {
                    if (_exitCode != ExitCode::Ok && _exitCode != ExitCode::DataError && _exitCode != ExitCode::InvalidToken &&
                        (_exitCode != ExitCode::BackError || _exitCause != ExitCause::NotFound)) {
                        LOG_WARN(_logger, "Error handling failed");
                    }
                    res = false;
                }
            }
            break;
        }
    }

    return res;
}

bool AbstractNetworkJob::followRedirect(std::istream &inputStream) {
    // Extract redirect URL
    Poco::XML::InputSource inputSrc(inputStream);
    Poco::XML::DOMParser parser;
    Poco::AutoPtr<Poco::XML::Document> pDoc;
    try {
        pDoc = parser.parse(&inputSrc);
    } catch (Poco::Exception &e) {
        LOG_DEBUG(_logger, "Reply " << jobId() << " received doesn't contain a valid JSON error: " << errorText(e).c_str());
        Utility::logGenericServerError(_logger, "Redirection error", inputStream, _resHttp);

        _exitCode = ExitCode::BackError;
        _exitCause = ExitCause::ApiErr;
        return false;
    }

    std::string redirectUrl;
    Poco::XML::Node *pNode = pDoc->getNodeByPath(redirectUrlPathKey);
    if (pNode != nullptr) {
        redirectUrl = pNode->innerText();
    }

    if (redirectUrl.empty()) {
        LOG_WARN(_logger, "Request " << jobId() << ": Failed to retrieve redirection URL");
        _exitCode = ExitCode::DataError;
        _exitCause = ExitCause::RedirectionError;
        return false;
    }

    Poco::URI uri(redirectUrl);

    // Follow redirection
    LOG_DEBUG(_logger, "Request " << jobId() << ", following redirection: " << redirectUrl.c_str());
    createSession(uri);

    if (!sendRequest(uri)) {
        return false;
    }
    bool receiveOk = receiveResponse(uri);
    if (!receiveOk && _resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        // Special cases where the file exist in DB but not in storage
        _downloadImpossible = true;
        return true;
    }
    return receiveOk;
}

bool AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId) {
    const std::scoped_lock<std::recursive_mutex> lock(_mutexSession);
    if (_session) {
        int err = _session->socket().getError();
        std::string errMsg = Poco::Error::getMessage(err);
        return processSocketError(msg, jobId, err, errMsg);
    } else {
        return processSocketError(msg, jobId, 0, std::string());
    }
}

bool AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, const std::exception &e) {
    return processSocketError(msg, jobId, 0, e.what());
}

bool AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, const Poco::Exception &e) {
    return processSocketError(msg, jobId, e.code(), e.message());
}

bool AbstractNetworkJob::processSocketError(const std::string &msg, const UniqueId jobId, int err, const std::string &errMsg) {
    if (isAborted()) {
        _exitCode = ExitCode::Ok;
        return true;
    } else {
        std::stringstream errMsgStream;
        errMsgStream << msg.c_str();
        if (jobId) errMsgStream << " - job " << jobId;
        if (err) errMsgStream << " : (" << err << ")";
        if (!errMsg.empty()) errMsgStream << " : " << errMsg.c_str();
        LOG_WARN(_logger, errMsgStream.str().c_str());

        _exitCode = ExitCode::NetworkError;
        if (err == EBADF) {
            // !!! macOS
            // When too many sockets are opened, the kernel kills all the process' sockets!
            // Console message generated: "mbuf_watchdog_defunct: defuncting all sockets from kDrive.<process id>"
            // macOS !!!
            LOG_WARN(_logger, "Sockets defuncted by kernel");
            _exitCause = ExitCause::SocketsDefuncted;
        } else if (err == POCO_ECONNREFUSED) {
            _exitCause = ExitCause::ConnectionRefused;
        } else {
            _exitCause = ExitCause::Unknown;
        }

        return false;
    }
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

bool AbstractNetworkJob::handleJsonResponse(std::istream &is) {
    return extractJson(is, _jsonRes);
}

bool AbstractNetworkJob::handleOctetStreamResponse(std::istream &is) {
    getStringFromStream(is, _octetStreamRes);
    return true;
}

bool AbstractNetworkJob::extractJson(std::istream &is, Poco::JSON::Object::Ptr &jsonObj) {
    try {
        jsonObj = Poco::JSON::Parser{}.parse(is).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception &exc) {
        LOGW_WARN(_logger, L"Reply " << jobId() << L" received doesn't contain a valid JSON payload: "
                                     << Utility::s2ws(exc.displayText()).c_str());
        Utility::logGenericServerError(_logger, "Request error", is, _resHttp);
        _exitCode = ExitCode::BackError;
        _exitCause = ExitCause::ApiErr;
        return false;
    }

    if (isExtendedLog()) {
        std::ostringstream os;
        jsonObj->stringify(os);
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << Utility::s2ws(os.str()).c_str());
    }
    return true;
}

bool AbstractNetworkJob::extractJsonError(std::istream &is, Poco::JSON::Object::Ptr errorObjPtr /*= nullptr*/) {
    Poco::JSON::Object::Ptr jsonObj;
    if (!extractJson(is, jsonObj)) return false;

    errorObjPtr = jsonObj->getObject(errorKey);
    if (!JsonParserUtility::extractValue(errorObjPtr, codeKey, _errorCode)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(errorObjPtr, descriptionKey, _errorDescr)) {
        return false;
    }

    return true;
}

void AbstractNetworkJob::TimeoutHelper::add(std::chrono::duration<double> duration) {
    unsigned int roundDuration = lround(duration.count() / PRECISION) * PRECISION;
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
    return static_cast<int>(_eventsQueue.size());
}

} // namespace KDC
