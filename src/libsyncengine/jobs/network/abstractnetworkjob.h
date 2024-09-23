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

#pragma once

#include "jobs/abstractjob.h"

#include <string>
#include <unordered_map>
#include <queue>

#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/JSON/Object.h>
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {

class AbstractJob;

class AbstractNetworkJob : public AbstractJob {
    public:
        AbstractNetworkJob();
        ~AbstractNetworkJob() override;

        bool hasHttpError() const;
        bool hasErrorApi(std::string *errorCode = nullptr, std::string *errorDescr = nullptr) const;
        [[nodiscard]] inline Poco::Net::HTTPResponse::HTTPStatus getStatusCode() const { return _resHttp.getStatus(); }
        void abort() override;

        [[nodiscard]] inline bool isDownloadImpossible() const { return _downloadImpossible; }

        inline std::string octetStreamRes() { return _octetStreamRes; }
        inline Poco::JSON::Object::Ptr jsonRes() { return _jsonRes; }

    protected:
        void runJob() noexcept override;
        virtual void addRawHeader(const std::string &key, const std::string &value) final;

        virtual bool handleResponse(std::istream &inputStream) = 0;
        /**
         * Return true if the error has been automatically resolved (e.g.: token expired)
         * Otherwise return false
         */
        virtual bool handleError(std::istream &inputStream, const Poco::URI &uri) = 0;

        virtual std::string getSpecificUrl() = 0;
        virtual std::string getUrl() = 0;

        void unzip(std::istream &inputStream, std::stringstream &ss);
        void getStringFromStream(std::istream &inputStream, std::string &res);

        [[nodiscard]] std::string errorText(Poco::Exception const &e) const;
        [[nodiscard]] std::string errorText(std::exception const &e) const;

        inline void noRetry() { _trials = 0; }

        virtual bool handleJsonResponse(std::istream &is);
        virtual bool handleOctetStreamResponse(std::istream &is);
        bool extractJson(std::istream &is, Poco::JSON::Object::Ptr &jsonObj);
        bool extractJsonError(std::istream &is, Poco::JSON::Object::Ptr errorObjPtr = nullptr);

        std::string _httpMethod;
        std::string _data;
        Poco::Net::HTTPResponse _resHttp;
        int _customTimeout = 0;
        int _trials = 2; // By default, try again once if exception is thrown
        std::string _errorCode;
        std::string _errorDescr;

    private:
        struct TimeoutHelper {
                void add(std::chrono::duration<double> duration);

                [[nodiscard]] inline int value() const { return _maxDuration; }
                inline bool isTimeoutDetected() { return count() >= TIMEOUT_THRESHOLD; }

            private:
                static const unsigned int DURATION_THRESHOLD = 15; // Start value of the duration detection threshold (sec)
                static const unsigned int PRECISION = 2; // Precision of the duration (sec) => duration interval = [_maxDuration
                                                         // - PRECISION, _maxDuration + PRECISION]
                static const unsigned int PERIOD = 600; // Sliding period of observation (sec)
                static const unsigned int TIMEOUT_THRESHOLD = 10; // Network timeout detection threshold (nbr of events)

                unsigned int _maxDuration = DURATION_THRESHOLD;
                std::queue<SyncTime> _eventsQueue;
                std::mutex _mutexEventsQueue;

                void clearAllEvents();
                void deleteOldestEvents();
                unsigned int count();
        };

        static std::string _userAgent;
        static Poco::Net::Context::Ptr _context;
        static TimeoutHelper _timeoutHelper;

        Poco::JSON::Object::Ptr _jsonRes{nullptr};
        std::string _octetStreamRes;

        virtual void setQueryParameters(Poco::URI &, bool &canceled) = 0;
        virtual void setData(bool &canceled) = 0;

        virtual std::string getContentType(bool &canceled) = 0;

        std::unique_ptr<Poco::Net::HTTPSClientSession> _session;
        std::recursive_mutex _mutexSession;

        void createSession(const Poco::URI &uri);
        void clearSession();
        void abortSession();
        bool sendRequest(const Poco::URI &uri);
        bool receiveResponse(const Poco::URI &uri);
        bool followRedirect(std::istream &inputStream);
        bool processSocketError(const std::string &msg, const UniqueId jobId);
        bool processSocketError(const std::string &msg, const UniqueId jobId, const std::exception &e);
        bool processSocketError(const std::string &msg, const UniqueId jobId, const Poco::Exception &e);
        bool processSocketError(const std::string &msg, const UniqueId jobId, int err, const std::string &errMsg);
        bool ioOrLogicalErrorOccurred(std::ios &stream);
        static bool isManagedError(ExitCode exitCode, ExitCause exitCause) noexcept;

        std::unordered_map<std::string, std::string> _rawHeaders;

        bool _downloadImpossible{false};
};

} // namespace KDC
