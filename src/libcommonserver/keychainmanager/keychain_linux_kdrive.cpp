/*
 * Copyright (c) 2013 GitHub Inc.
 * Copyright (c) 2015-2019 Vaclav Slavik
 * Copyright (c) 2019 Hannes Rantzsch, René Meusel
 * Copyright (C) 2026 Infomaniak Network SA
 *
 * Derived from hrantzsch/keychain src/keychain_linux.cpp.
 * Modified by Infomaniak Network SA for kDrive to make libsecret calls cancellable and bounded.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "keychain.h"

#include <libsecret/secret.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {

const char *ServiceFieldName = "service";
const char *AccountFieldName = "username";
const auto LibSecretTimeout = std::chrono::seconds(10);

// disable warnings about missing initializers in SecretSchema
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

SecretSchema makeSchema(const std::string &package) {
    return SecretSchema{package.c_str(),
                        SECRET_SCHEMA_NONE,
                        {
                                {ServiceFieldName, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                {AccountFieldName, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                {NULL, SecretSchemaAttributeType(0)},
                        }};
}

std::string makeLabel(const std::string &service, const std::string &user) {
    std::string label = service;

    if (!user.empty()) {
        label += " (" + user + ")";
    }

    return label;
}

void updateError(keychain::Error &err, GError *error) {
    if (error == NULL) {
        err = keychain::Error{};
        return;
    }

    err.type = keychain::ErrorType::GenericError;
    err.message = error->message;
    err.code = error->code;
    g_error_free(error);
}

void setErrorNotFound(keychain::Error &err) {
    err.type = keychain::ErrorType::NotFound;
    err.message = "Password not found.";
    err.code = -1; // generic non-zero
}

void setErrorTimeout(keychain::Error &err) {
    err.type = keychain::ErrorType::GenericError;
    err.message = "Secret Service did not respond within 10 seconds.";
    err.code = -2; // kDrive timeout
}

class CancellableTimeout {
    public:
        CancellableTimeout() :
            _cancellable(g_cancellable_new()),
            _thread([this] { run(); }) {}

        ~CancellableTimeout() {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _finished = true;
            }
            _condition.notify_one();
            if (_thread.joinable()) {
                _thread.join();
            }
            g_object_unref(_cancellable);
        }

        CancellableTimeout(const CancellableTimeout &) = delete;
        CancellableTimeout &operator=(const CancellableTimeout &) = delete;

        GCancellable *cancellable() const { return _cancellable; }

        bool timedOut() const {
            std::lock_guard<std::mutex> lock(_mutex);
            return _timedOut;
        }

    private:
        void run() {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_condition.wait_for(lock, LibSecretTimeout, [this] { return _finished; })) {
                return;
            }

            _timedOut = true;
            g_cancellable_cancel(_cancellable);
        }

        GCancellable *_cancellable = nullptr;
        mutable std::mutex _mutex;
        std::condition_variable _condition;
        bool _finished = false;
        bool _timedOut = false;
        std::thread _thread;
};

} // namespace

namespace keychain {

void setPassword(const std::string &package, const std::string &service, const std::string &user, const std::string &password,
                 Error &err) {
    err = Error{};
    const auto schema = makeSchema(package);
    const auto label = makeLabel(service, user);
    GError *error = NULL;
    CancellableTimeout timeout;

    secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT, label.c_str(), password.c_str(), timeout.cancellable(), &error,
                               ServiceFieldName, service.c_str(), AccountFieldName, user.c_str(), NULL);

    if (timeout.timedOut()) {
        if (error != NULL) {
            g_error_free(error);
        }
        setErrorTimeout(err);
    } else if (error != NULL) {
        updateError(err, error);
    }
}

std::string getPassword(const std::string &package, const std::string &service, const std::string &user, Error &err) {
    err = Error{};
    const auto schema = makeSchema(package);
    GError *error = NULL;
    CancellableTimeout timeout;

    gchar *raw_passwords = secret_password_lookup_sync(&schema, timeout.cancellable(), &error, ServiceFieldName, service.c_str(),
                                                       AccountFieldName, user.c_str(), NULL);

    std::string password;

    if (timeout.timedOut()) {
        if (error != NULL) {
            g_error_free(error);
        }
        if (raw_passwords != NULL) {
            secret_password_free(raw_passwords);
        }
        setErrorTimeout(err);
    } else if (error != NULL) {
        updateError(err, error);
    } else if (raw_passwords == NULL) {
        // libsecret reports no error if the password was not found
        setErrorNotFound(err);
    } else {
        password = raw_passwords;
        secret_password_free(raw_passwords);
    }

    return password;
}

void deletePassword(const std::string &package, const std::string &service, const std::string &user, Error &err) {
    err = Error{};
    const auto schema = makeSchema(package);
    GError *error = NULL;
    CancellableTimeout timeout;

    bool deleted = secret_password_clear_sync(&schema, timeout.cancellable(), &error, ServiceFieldName, service.c_str(),
                                              AccountFieldName, user.c_str(), NULL);

    if (timeout.timedOut()) {
        if (error != NULL) {
            g_error_free(error);
        }
        setErrorTimeout(err);
    } else if (error != NULL) {
        updateError(err, error);
    } else if (!deleted) {
        // libsecret reports no error if the password did not exist
        setErrorNotFound(err);
    }
}

} // namespace keychain
