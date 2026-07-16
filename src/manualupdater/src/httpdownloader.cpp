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

#include <sstream>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPSClientSession.h>

namespace KDUpdater {

HttpDownloader::Result HttpDownloader::get(const std::string &url) {
    Result result;
    try {
        Poco::URI uri(url);

        Poco::Net::Context::Ptr context =
                new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
        context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);

        Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort(), context);
        session.setTimeout(Poco::Timespan(30, 0));

        std::string path = uri.getPathAndQuery();
        if (path.empty()) {
            path = "/";
        }

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", KDC::CommonUtility::userAgentString());
        request.set("Accept", "application/json");

        std::ostream &reqStream = session.sendRequest(request);
        (void) reqStream; // HTTP GET has no body

        Poco::Net::HTTPResponse response;
        std::istream &respStream = session.receiveResponse(response);

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

bool HttpDownloader::fetchAppVersion(KDC::DistributionChannel channel, const std::string &appId, KDC::VersionInfo &outVersionInfo,
                                     std::string &outError) {
    constexpr auto kEndpoint = "/app-information/applications/version/no-auth";

    try {
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
