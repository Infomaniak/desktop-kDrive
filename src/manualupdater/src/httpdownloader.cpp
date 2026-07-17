/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
 */

#include "httpdownloader.h"

#include "libcommon/utility/utility.h"
#include "libcommon/utility/urlhelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/Context.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPSClientSession.h>

#include <Poco/JSON/Parser.h>
#include <fstream>
#include <memory>
#include <sstream>

namespace KDUpdater {

namespace {

Poco::Net::Context::Ptr createSslContext() {
    Poco::Net::Context::Ptr context =
            new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
    context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
    return context;
}

inline std::unique_ptr<Poco::Net::HTTPSClientSession> createHttpsSession(const std::string &url, Poco::Timespan timeout,
                                                                         std::string &outPath) {
    Poco::URI uri(url);
    auto session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), createSslContext());
    session->setTimeout(timeout);

    outPath = uri.getPathAndQuery();
    if (outPath.empty()) {
        outPath = "/";
    }
    return session;
}

} // namespace

HttpDownloader::Result HttpDownloader::get(const std::string &url) {
    Result result;
    try {
        std::string path;
        auto session = createHttpsSession(url, Poco::Timespan(30, 0), path);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", KDC::CommonUtility::userAgentString());
        request.set("Accept", "application/json");

        session->sendRequest(request);

        Poco::Net::HTTPResponse response;
        std::istream &respStream = session->receiveResponse(response);

        result.statusCode = static_cast<int>(response.getStatus());

        std::string body(std::istreambuf_iterator<char>(respStream), (std::istreambuf_iterator<char>()));
        result.body = std::move(body);

        if (result.statusCode == Poco::Net::HTTPResponse::HTTP_OK) {
            result.success = true;
        } else {
            result.error = "HTTP " + std::to_string(result.statusCode) + " " + response.getReason();
        }
    } catch (const Poco::Exception &e) {
        result.error = e.displayText();
    } catch (const std::exception &e) {
        result.error = e.what();
    } catch (...) {
        result.error = "unknown exception";
    }
    return result;
}

HttpDownloader::Result HttpDownloader::downloadFile(const std::string &url, const KDC::SyncPath &destPath, long timeoutSeconds) {
    Result result;
    try {
        std::string path;
        auto session = createHttpsSession(url, Poco::Timespan(timeoutSeconds, 0), path);

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", KDC::CommonUtility::userAgentString());

        session->sendRequest(request);

        Poco::Net::HTTPResponse response;
        std::istream &respStream = session->receiveResponse(response);

        result.statusCode = static_cast<int>(response.getStatus());

        if (result.statusCode != Poco::Net::HTTPResponse::HTTP_OK) {
            result.error = "HTTP " + std::to_string(result.statusCode) + " " + response.getReason();
            return result;
        }

        std::ofstream outFile(destPath, std::ios::binary);
        if (!outFile) {
            result.error = "Failed to open file for writing: " + destPath.string();
            return result;
        }

        std::array<char, 8192> buffer{};
        while (respStream.read(buffer.data(), buffer.size()) || respStream.gcount() > 0) {
            outFile.write(buffer.data(), respStream.gcount());
            if (!outFile) {
                result.error = "Failed to write to file: " + destPath.string();
                outFile.close();
                return result;
            }
        }
        outFile.close();
        result.success = true;
    } catch (const Poco::Exception &e) {
        result.error = e.displayText();
    } catch (const std::exception &e) {
        result.error = e.what();
    } catch (...) {
        result.error = "unknown exception";
    }
    return result;
}

bool HttpDownloader::fetchAppVersion(KDC::DistributionChannel channel, const std::string &appId, KDC::VersionInfo &outVersionInfo,
                                     std::string &outError) {
    try {
        constexpr auto kEndpoint = "/app-information/applications/version/no-auth";
        Poco::URI uri(KDC::UrlHelper::infomaniakApiUrl(1) + kEndpoint);
        uri.addQueryParameter("appId", appId);
        uri.addQueryParameter("channel", KDC::toString(channel));
        uri.addQueryParameter("platform", KDC::toString(KDC::CommonUtility::platform()));
        uri.addQueryParameter("os_version", KDC::CommonUtility::osVersion());
        uri.addQueryParameter("store", "kStore");
        uri.addQueryParameter("name", "com.infomaniak.drive");

        auto result = get(uri.toString());
        if (!result.success) {
            outError = result.error;
            return false;
        }

        Poco::JSON::Object::Ptr jsonObj = Poco::JSON::Parser{}.parse(result.body).extract<Poco::JSON::Object::Ptr>();
        Poco::JSON::Object::Ptr dataObj = jsonObj->getObject("data");
        if (!dataObj) {
            outError = "missing 'data' key in response";
            LOG_WARN(KDC::Log::instance()->getLogger(), outError);
            return false;
        }

        std::string channelStr;
        if (!KDC::JsonParserUtility::extractValue(dataObj, "channel", channelStr)) {
            outError = "missing 'channel'";
            return false;
        }
        outVersionInfo.channel = KDC::toDistributionChannel(channelStr);

        if (!KDC::JsonParserUtility::extractValue(dataObj, "tag", outVersionInfo.tag)) {
            outError = "missing 'tag'";
            return false;
        }
        if (!KDC::JsonParserUtility::extractValue(dataObj, "build_version", outVersionInfo.buildVersion)) {
            outError = "missing 'build_version'";
            return false;
        }
        if (!KDC::JsonParserUtility::extractValue(dataObj, "build_min_os_version", outVersionInfo.minOsVersion)) {
            outError = "missing 'build_min_os_version'";
            return false;
        }
        if (!KDC::JsonParserUtility::extractValue(dataObj, "download_link", outVersionInfo.downloadUrl)) {
            outError = "missing 'download_link'";
            return false;
        }
        if (!KDC::JsonParserUtility::extractValue(dataObj, "checksum", outVersionInfo.checksum)) {
            outError = "missing 'checksum'";
            return false;
        }
        if (!KDC::JsonParserUtility::extractValue(dataObj, "min_version", outVersionInfo.minAppVersion)) {
            outError = "missing 'min_version'";
            return false;
        }

        return true;
    } catch (const Poco::Exception &e) {
        outError = e.displayText();
        return false;
    } catch (const std::exception &e) {
        outError = e.what();
        return false;
    } catch (...) {
        outError = "unknown exception";
        return false;
    }
}

} // namespace KDUpdater
