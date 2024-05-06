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

#include <iostream>  // std::ios, std::istream, std::cout, std::cerr
#include <functional>

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
                    LOG_INFO(_logger, "Error in Poco::Net::Context constructor: " << e.displayText().c_str() << " (" << e.code()
                                                                                  << "), retrying...");
                    continue;
                } else {
                    LOG_INFO(_logger,
                             "Error in Poco::Net::Context constructor: " << e.displayText().c_str() << " (" << e.code() << ")");
                    throw std::runtime_error(ABSTRACTNETWORKJOB_NEW_ERROR_MSG);
                    break;
                }
            } catch (...) {
                if (trials < _trials) {
                    LOG_INFO(_logger, "Unknown error in Poco::Net::Context constructor, retrying...");
                } else {
                    LOG_ERROR(_logger, "Unknown error in Poco::Net::Context constructor");
                    throw std::runtime_error(ABSTRACTNETWORKJOB_NEW_ERROR_MSG);
                }
            }
        }
    }
}


bool AbstractNetworkJob::isManagedError(ExitCode exitCode, ExitCause exitCause) noexcept {
    static const std::set<ExitCause> managedExitCauses = {ExitCauseInvalidName,   ExitCauseApiErr,
                                                          ExitCauseFileTooBig,    ExitCauseNotFound,
                                                          ExitCauseQuotaExceeded, ExitCauseFileAlreadyExist};

    switch (exitCode) {
        case ExitCodeBackError:
            return managedExitCauses.find(exitCause) != managedExitCauses.cend();
        case ExitCodeNetworkError:
            return exitCause == ExitCauseNetworkTimeout;
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
            Utility::msleep(500);  // Sleep for 0.5s
        }

        uri = Poco::URI(url);
        Poco::Net::HTTPSClientSession session = createSession(uri);
        {
            const std::lock_guard<std::mutex> lock(_mutexSession);
            _session = &session;
        }
        if (_customTimeout) {
            session.setTimeout(Poco::Timespan(_customTimeout, 0));
        }

        // Set proxy params
        if (Proxy::instance()->proxyConfig().type() == ProxyTypeHTTP) {
            session.setProxy(Proxy::instance()->proxyConfig().hostName(), Proxy::instance()->proxyConfig().port());
            if (Proxy::instance()->proxyConfig().needsAuth()) {
                session.setProxyCredentials(Proxy::instance()->proxyConfig().user(), Proxy::instance()->proxyConfig().token());
            }
        }

        try {
            if (!canRun()) {
                const std::lock_guard<std::mutex> lock(_mutexSession);
                _session = nullptr;
                LOG_INFO(_logger, "Session for job " << jobId() << " set to nullptr");
                return;
            }
        } catch (Poco::Exception const &e) {
            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str() << " " << errorText(e).c_str());
            _exitCode = ExitCodeNetworkError;
            break;
        }

        bool canceled = false;
        setData(canceled);  // Must be called before setQueryParameters
        if (canceled) {
            LOG_WARN(_logger, "Job " << jobId() << " is cancelled");
            _exitCode = ExitCodeDataError;
            break;
        }

        setQueryParameters(uri, canceled);
        if (canceled) {
            LOG_WARN(_logger, "Job " << jobId() << " is cancelled");
            _exitCode = ExitCodeDataError;
            break;
        }

        // Send request
        auto sendChrono = std::chrono::steady_clock::now();
        bool ret = false;
        try {
            ret = sendRequest(session, uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in sendRequest " << jobId() << " err=" << e.what());
            _exitCode = ExitCodeNetworkError;
            ret = false;
        }

        if (!ret) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString().c_str() << " aborted");
                _exitCode = ExitCodeOk;
                break;
            }

            if (_exitCode == ExitCodeNetworkError && _exitCause == ExitCauseSocketsDefuncted) {
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
            ret = receiveResponse(session, uri);
        } catch (std::exception &e) {
            LOG_WARN(_logger, "Error in receiveResponse " << jobId() << " err=" << e.what());
            _exitCode = ExitCodeNetworkError;
            ret = false;
        }

        if (!ret) {
            if (isAborted()) {
                LOG_INFO(_logger, "Request " << jobId() << " " << uri.toString().c_str() << " aborted");
                _exitCode = ExitCodeOk;
                break;
            }

            // Attempt to detect network timeout
            auto errChrono = std::chrono::steady_clock::now();
            std::chrono::duration<double> requestDuration = errChrono - sendChrono;
            if (_exitCode == ExitCodeNetworkError) {
                _timeoutHelper.add(requestDuration);
                if (_timeoutHelper.isTimeoutDetected()) {
                    LOG_WARN(_logger, "Network timeout detected - value=" << _timeoutHelper.value());
                    _exitCause = ExitCauseNetworkTimeout;
                }
            }

            if (trials < _trials) {
                LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str() << ", retrying...");
                continue;
            }

            LOG_INFO(_logger, "Error with request " << jobId() << " " << uri.toString().c_str());
            break;
        }

        if (_exitCode == ExitCodeTokenRefreshed || _exitCode == ExitCodeRateLimited) {
            _exitCode = ExitCodeOk;
            _trials++;  // Add one more chance
            continue;
        } else if (isManagedError(_exitCode, _exitCause)) {
            break;
        } else {
            _exitCode = ExitCodeOk;
            break;
        }
    }

    if (!isAborted()) {
        const std::lock_guard<std::mutex> lock(_mutexSession);
        _session = nullptr;
    }
}

bool AbstractNetworkJob::hasHttpError() {
    if (_resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
        return false;
    }
    return true;
}

void AbstractNetworkJob::addRawHeader(const std::string &key, const std::string &value) {
    _rawHeaders.insert_or_assign(key, value);
}

void AbstractNetworkJob::abort() {
    LOG_DEBUG(_logger, "Aborting session for job " << jobId());

    AbstractJob::abort();
    const std::lock_guard<std::mutex> lock(_mutexSession);
    if (_session) {
        Poco::Net::SocketImpl *socketImpl = _session->socket().impl();
        if (socketImpl) {
            if (socketImpl->sockfd()) {
                try {
                    _session->abort();
                } catch (std::exception &e) {
                    LOG_DEBUG(_logger, "Job " << jobId() << " abort error - err=" << e.what());
                }
            } else {
                LOG_DEBUG(_logger, "Job " << jobId() << " already aborted");
            }
        }
        _session = nullptr;
    }
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

Poco::Net::HTTPSClientSession AbstractNetworkJob::createSession(const Poco::URI &uri) {
    return Poco::Net::HTTPSClientSession(uri.getHost(), uri.getPort(), _context);
}

bool AbstractNetworkJob::sendRequest(Poco::Net::HTTPSClientSession &session, const Poco::URI &uri) {
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
        _exitCode = ExitCodeDataError;
        return false;
    }

    Poco::Net::HTTPRequest req(_httpMethod, path, Poco::Net::HTTPMessage::HTTP_1_1);

    // Set headers
    req.set("User-Agent", _userAgent);
    req.setContentType(contentType);
    for (const auto &header : _rawHeaders) {
        req.add(header.first, header.second);
    }

    if (_data.size() != 0) {
        req.setContentLength(_data.size());
    }

    // Send request, retrieve an open stream
    std::vector<std::reference_wrapper<std::ostream>> stream;
    try {
        stream.push_back(session.sendRequest(req));
        if (ioOrLogicalErrorOccurred(stream[0].get())) {
            int err = session.socket().getError();
            return processSocketError(session, "sendRequest failed failed", jobId(), err, Poco::Error::getMessage(err));
        }
    } catch (Poco::Exception &e) {
        return processSocketError(session, "sendRequest exception", jobId(), e.code(), e.message());
    } catch (std::exception &e) {
        return processSocketError(session, "sendRequest exception", jobId(), 0, e.what());
    }

    // Send data
    std::string::const_iterator itBegin = _data.begin();
    while (itBegin != _data.end()) {
        if (isAborted()) {
            LOG_DEBUG(_logger, "Request " << jobId() << ": aborting HTTPS session");
            return false;
        }

        std::string::const_iterator itEnd = (_data.end() - itBegin > BUF_SIZE ? itBegin + BUF_SIZE : _data.end());
        try {
            stream[0].get() << std::string(itBegin, itEnd);
            if (ioOrLogicalErrorOccurred(stream[0].get())) {
                int err = session.socket().getError();
                return processSocketError(session, "send data failed", jobId(), err, Poco::Error::getMessage(err));
            }
        } catch (Poco::Exception &e) {
            return processSocketError(session, "send data exception", jobId(), e.code(), e.message());
        } catch (std::exception &e) {
            return processSocketError(session, "send data exception", jobId(), 0, e.what());
        }

        if (isProgressTracked()) {
            _progress += itEnd - itBegin;
        }

        itBegin = itEnd;
    }

    return true;
}

bool AbstractNetworkJob::receiveResponse(Poco::Net::HTTPSClientSession &session, const Poco::URI &uri) {
    std::vector<std::reference_wrapper<std::istream>> stream;
    try {
        stream.push_back(session.receiveResponse(_resHttp));
        if (ioOrLogicalErrorOccurred(stream[0].get())) {
            int err = session.socket().getError();
            return processSocketError(session, "receiveResponse failed", jobId(), err, Poco::Error::getMessage(err));
        }
    } catch (Poco::Exception &e) {
        return processSocketError(session, "receiveResponse exception", jobId(), e.code(), e.message());
    } catch (std::exception &e) {
        return processSocketError(session, "receiveResponse exception", jobId(), 0, e.what());
    }

    LOG_DEBUG(_logger, "Request " << jobId() << " finished with status: " << _resHttp.getStatus() << " / "
                                  << _resHttp.getReason().c_str());

    bool res = true;
    switch (_resHttp.getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_OK: {
            bool ok = false;
            try {
                ok = handleResponse(stream[0].get());
            } catch (std::exception &e) {
                LOG_WARN(_logger, "handleResponse failed - err= " << e.what());
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
                    if (_exitCode != ExitCodeOk && _exitCode != ExitCodeDataError) {
                        LOG_WARN(_logger, "Redirect handling failed");
                    }
                    return false;
                }
                return true;
            }
            break;
        }
        case Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS: {
            // Rate limitation
            _exitCode = ExitCodeRateLimited;
        }
        default: {
            if (!isAborted()) {
                bool ok = false;
                try {
                    ok = handleError(stream[0].get(), uri);
                } catch (std::exception &e) {
                    LOG_WARN(_logger, "handleError failed - err= " << e.what());
                    return false;
                }

                if (!ok) {
                    if (_exitCode != ExitCodeOk && _exitCode != ExitCodeDataError && _exitCode != ExitCodeInvalidToken &&
                        (_exitCode != ExitCodeBackError || _exitCause != ExitCauseNotFound)) {
                        LOG_WARN(_logger, "Error handling failed");
                    }
                    res = false;
                }
            }
            break;
        }
    }

    if (isAborted()) {
        LOG_DEBUG(_logger, "Request " << jobId() << ": aborting session");
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
    } catch (Poco::Exception &exc) {
        LOG_DEBUG(_logger, "Reply " << jobId() << " received doesn't contain a valid JSON error: " << exc.displayText().c_str());
        Utility::logGenericServerError("Redirection error", inputStream, _resHttp);

        _exitCode = ExitCodeBackError;
        _exitCause = ExitCauseApiErr;
        return false;
    }

    std::string redirectUrl;
    Poco::XML::Node *pNode = pDoc->getNodeByPath(redirectUrlPathKey);
    if (pNode != nullptr) {
        redirectUrl = pNode->innerText();
    }

    if (redirectUrl.empty()) {
        LOG_WARN(_logger, "Request " << jobId() << ": Failed to retrieve redirection URL");
        _exitCode = ExitCodeDataError;
        _exitCause = ExitCauseRedirectionError;
        return false;
    }

    Poco::URI uri(redirectUrl);

    LOG_DEBUG(_logger, "Request " << jobId() << ", following redirection: " << redirectUrl.c_str());
    // Follow redirection
    Poco::Net::HTTPSClientSession session = createSession(uri);
    {
        const std::lock_guard<std::mutex> lock(_mutexSession);
        _session = &session;
    }
    if (_customTimeout) {
        session.setTimeout(Poco::Timespan(_customTimeout, 0));
    }

    if (!sendRequest(session, uri)) {
        return false;
    }
    bool receiveOk = receiveResponse(session, uri);
    if (!receiveOk && _resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        // Special cases where the file exist in DB but not in storage
        _downloadImpossible = true;
        return true;
    }
    return receiveOk;
}

bool AbstractNetworkJob::processSocketError(Poco::Net::HTTPSClientSession &session, const std::string &msg, const UniqueId jobId,
                                            int err /*= 0*/, const std::string &errMsg /*= std::string()*/) {
    const std::lock_guard<std::mutex> lock(_mutexSession);
    session.reset();
    _session = nullptr;

    if (isAborted()) {
        _exitCode = ExitCodeOk;
        return true;
    } else {
        std::stringstream errMsgStream;
        errMsgStream << msg.c_str();
        if (jobId) errMsgStream << " - job ID= " << jobId;
        if (err) errMsgStream << " - err= " << jobId;
        if (!errMsg.empty()) errMsgStream << " - err message= " << errMsg.c_str();
        LOG_WARN(_logger, &errMsgStream);

        _exitCode = ExitCodeNetworkError;
        if (err == EBADF) {
            // !!! macOS
            // When too many sockets are opened, the kernel kills all the process' sockets!
            // Console message generated: "mbuf_watchdog_defunct: defuncting all sockets from kDrive.<process id>"
            // macOS !!!
            _exitCause = ExitCauseSocketsDefuncted;
        } else {
            _exitCause = ExitCauseUnknown;
        }
        return false;
    }
}

bool AbstractNetworkJob::ioOrLogicalErrorOccurred(std::ios &stream) {
    bool res = (stream.fail() || stream.bad()) && !stream.eof();
    if (res) {
        LOG_DEBUG(_logger, "sendRequest failed for job " << jobId() << " - stream fail=" << stream.fail()
                                                         << " - stream bad=" << stream.bad() << " - stream eof=" << stream.eof());
    }
    return res;
}

const std::string AbstractNetworkJob::errorText(const Poco::Exception &e) const {
    std::ostringstream error;
    error << e.className() << " : " << e.code() << " : " << e.displayText().c_str();
    return error.str();
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
        const std::lock_guard<std::mutex> lock(_mutexEventsQueue);
        unsigned int eventTime = static_cast<unsigned int>(time(NULL));
        _eventsQueue.push(eventTime);
    }
}

void AbstractNetworkJob::TimeoutHelper::clearAllEvents() {
    const std::lock_guard<std::mutex> lock(_mutexEventsQueue);
    std::queue<SyncTime> emptyQueue;
    std::swap(_eventsQueue, emptyQueue);
}

void AbstractNetworkJob::TimeoutHelper::deleteOldestEvents() {
    const std::lock_guard<std::mutex> lock(_mutexEventsQueue);
    if (!_eventsQueue.empty()) {
        unsigned int eventTime = static_cast<unsigned int>(time(NULL));
        while (eventTime - _eventsQueue.front() > PERIOD) {
            LOG_DEBUG(Log::instance()->getLogger(), "TimeoutHelper - Clear event " << _eventsQueue.front());
            _eventsQueue.pop();
        }
    }
}

unsigned int AbstractNetworkJob::TimeoutHelper::count() {
    deleteOldestEvents();
    return static_cast<int>(_eventsQueue.size());
}

}  // namespace KDC
