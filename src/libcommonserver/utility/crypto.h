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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <openssl/x509v3.h>
#include <openssl/err.h>

namespace KDC {

struct X509Deleter {
        void operator()(X509 *const x) const { X509_free(x); }
};
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

struct EvpPkeyDeleter {
        void operator()(EVP_PKEY *const p) const { EVP_PKEY_free(p); }
};
using UniqueEvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

struct BignumDeleter {
        void operator()(BIGNUM *const b) const { BN_free(b); }
};
using UniqueBignum = std::unique_ptr<BIGNUM, BignumDeleter>;

struct BioDeleter {
        void operator()(BIO *const x) const { BIO_free(x); }
};
using UniqueBio = std::unique_ptr<BIO, BioDeleter>;

inline const char *sslError() {
    const unsigned long err = ERR_get_error();
    return err ? ERR_error_string(err, nullptr) : "no OpenSSL error";
}

} // namespace KDC
