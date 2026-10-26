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

#include "testcertificateblocklist.h"

#include "libcommonserver/utility/certificateblocklist.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstdint>
#include <memory>

namespace KDC {

namespace {

// Certificates of the CVE-2026-111943 scenario, taken from OkHttp's CompromisedIntermediateTest.

// Subject: CN=emSign Root TLS CA - G3, O=eMudhra Technologies Limited, C=IN (self-signed root)
constexpr const char *emsignRootTlsCaG3Pem = R"(-----BEGIN CERTIFICATE-----
MIICKTCCAa+gAwIBAgIQDnYGcvFDRZ/I/gqwvAXjlDAKBggqhkjOPQQDAzBWMQsw
CQYDVQQGEwJJTjElMCMGA1UECgwcZU11ZGhyYSBUZWNobm9sb2dpZXMgTGltaXRl
ZDEgMB4GA1UEAwwXZW1TaWduIFJvb3QgVExTIENBIC0gRzMwHhcNMjQwMjA4MDgx
OTMyWhcNNDkwMjA3MDgxOTMyWjBWMQswCQYDVQQGEwJJTjElMCMGA1UECgwcZU11
ZGhyYSBUZWNobm9sb2dpZXMgTGltaXRlZDEgMB4GA1UEAwwXZW1TaWduIFJvb3Qg
VExTIENBIC0gRzMwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAARfD9w/q1cWxL6D9bMG
H6CQ4u+gAV6GN9s+PwQx9Bl4hC+0iU8QOk8mWO/RwXQy10DCSxT8vSIJClHKp+Ao
4qJKdYeV0SwZ7LMQziyLfxIUOY6Grja/sQFwLKYf8EdDXAyjQjBAMB0GA1UdDgQW
BBThOqshfPHjTVSu1HK3/YtZQiCQVDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/
BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjBhCFTkly7Yl7+s4hiw+k6ZJTXT2AiI
rvmsmfqPr5E7ecwXb6kXA5LTFd6rjwTuV3sCMQC9w6ZSbVq1mveMO6dKFziOzClT
KCT2RFT6yXtq6nzgRICgBmiXbJAXsmZ8yAwjyWc=
-----END CERTIFICATE-----
)";

// Subject: CN=EM OV TLS CA - G3C, O=eMudhra Inc, C=US, issued by the root above (compromised intermediate)
constexpr const char *emOvTlsCaG3cPem = R"(-----BEGIN CERTIFICATE-----
MIIDaDCCAu6gAwIBAgIPKWkmwiejoVJjkrGUgqUEMAoGCCqGSM49BAMDMFYxCzAJ
BgNVBAYTAklOMSUwIwYDVQQKDBxlTXVkaHJhIFRlY2hub2xvZ2llcyBMaW1pdGVk
MSAwHgYDVQQDDBdlbVNpZ24gUm9vdCBUTFMgQ0EgLSBHMzAeFw0yNTA0MzAxNTM3
NDFaFw00MDA0MjgxODMwMDBaMEAxCzAJBgNVBAYTAlVTMRQwEgYDVQQKDAtlTXVk
aHJhIEluYzEbMBkGA1UEAwwSRU0gT1YgVExTIENBIC0gRzNDMHYwEAYHKoZIzj0C
AQYFK4EEACIDYgAEE4JRzVKskpjByKrZdzId65fnCb0LTKCspV3IrVHc/J0ViaFZ
fjpRIOHv1jHGPhg1ysrimGmmLhYx6KKBgatWYW3EXZGKvAnzqw5jz3kqpNztc4e+
N7ulaVSfHAKycO1no4IBljCCAZIwHwYDVR0jBBgwFoAU4TqrIXzx401UrtRyt/2L
WUIgkFQwHQYDVR0OBBYEFDTq6gzryQra+dDF4u2beMmHnhtsMB0GA1UdJQQWMBQG
CCsGAQUFBwMBBggrBgEFBQcDAjBdBgNVHSAEVjBUMDkGCysGAQQBg44hAQABMCow
KAYIKwYBBQUHAgEWHGh0dHA6Ly9yZXBvc2l0b3J5LmVtc2lnbi5jb20wDQYLKwYB
BAGDjiEBAm4wCAYGZ4EMAQICMHUGCCsGAQUFBwEBBGkwZzAkBggrBgEFBQcwAYYY
aHR0cDovL29jc3AtYy5lbXNpZ24uY29tMD8GCCsGAQUFBzAChjNodHRwOi8vcmVw
b3NpdG9yeS5lbXNpZ24uY29tL2NlcnRzL0VNT1ZUTFNDQUczQy5wN2MwNwYDVR0f
BDAwLjAsoCqgKIYmaHR0cDovL2NybC5lbXNpZ24uY29tLz9yb290dGxzY2FnMy5j
cmwwDgYDVR0PAQH/BAQDAgEGMBIGA1UdEwEB/wQIMAYBAf8CAQAwCgYIKoZIzj0E
AwMDaAAwZQIxAMSZt2xsPeumRdeSMSQ3CbzIZeQ/q7GDCWfeE1Pq+mhgA6gRM44c
NVF3xJ+wsxJk+wIwFJbaDMokD8Ei6eNOiA8yUMYvfzQyngmrhZQMdPCK1SewrR79
4rSb9vGqxZzelQth
-----END CERTIFICATE-----
)";

// Private key of the intermediate above: the scalar 4 on secp384r1.
constexpr const char *emOvTlsCaG3cKeyPem = R"(-----BEGIN EC PRIVATE KEY-----
MD4CAQEEMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAABKAHBgUrgQQAIg==
-----END EC PRIVATE KEY-----
)";

struct X509Deleter {
        void operator()(X509 *const x) const { X509_free(x); }
};
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

struct EvpPkeyDeleter {
        void operator()(EVP_PKEY *const p) const { EVP_PKEY_free(p); }
};
using UniqueEvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

struct X509StoreDeleter {
        void operator()(X509_STORE *const s) const { X509_STORE_free(s); }
};
using UniqueX509Store = std::unique_ptr<X509_STORE, X509StoreDeleter>;

struct X509StoreCtxDeleter {
        void operator()(X509_STORE_CTX *const c) const { X509_STORE_CTX_free(c); }
};
using UniqueX509StoreCtx = std::unique_ptr<X509_STORE_CTX, X509StoreCtxDeleter>;

struct X509StackDeleter {
        void operator()(STACK_OF(X509) *const s) const { sk_X509_free(s); }
};
using UniqueX509Stack = std::unique_ptr<STACK_OF(X509), X509StackDeleter>;

struct BioDeleter {
        void operator()(BIO *const b) const { BIO_free_all(b); }
};
using UniqueBio = std::unique_ptr<BIO, BioDeleter>;

UniqueX509 loadCert(const char *const pem) {
    const UniqueBio bio(BIO_new_mem_buf(pem, -1));
    return UniqueX509(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
}

UniqueEvpPkey loadKey(const char *const pem) {
    const UniqueBio bio(BIO_new_mem_buf(pem, -1));
    return UniqueEvpPkey(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

// Builds a certificate for `key`, signed by `signingKey` on behalf of `issuer` (self-signed when null).
// Returns null if any step fails.
UniqueX509 makeCert(EVP_PKEY *const key, const char *const commonName, const X509 *const issuer, EVP_PKEY *const signingKey) {
    UniqueX509 cert(X509_new());
    if (!cert) return nullptr;
    if (X509_set_version(cert.get(), X509_VERSION_3) != 1 || ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1) != 1 ||
        !X509_gmtime_adj(X509_getm_notBefore(cert.get()), -3600) || !X509_gmtime_adj(X509_getm_notAfter(cert.get()), 3600) ||
        X509_set_pubkey(cert.get(), key) != 1) {
        return nullptr;
    }

    X509_NAME *const name = X509_get_subject_name(cert.get());
    const auto *const cn = reinterpret_cast<const unsigned char *>(commonName);
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, cn, -1, -1, 0) != 1 ||
        X509_set_issuer_name(cert.get(), issuer ? X509_get_subject_name(issuer) : name) != 1) {
        return nullptr;
    }

    if (X509_sign(cert.get(), signingKey, EVP_sha384()) <= 0) return nullptr;
    return cert;
}

// Runs CertificateBlocklist::verifyChain on `leaf`, with `trusted` as the only trust anchor (if any)
// and `intermediate` as an untrusted certificate sent by the peer (if any).
// Returns -1 if the verification context cannot be set up.
int32_t verify(X509 *const leaf, X509 *const trusted, X509 *const intermediate, int32_t &error) {
    const UniqueX509Store store(X509_STORE_new());
    if (!store || (trusted && X509_STORE_add_cert(store.get(), trusted) != 1)) return -1;

    const UniqueX509Stack untrusted(sk_X509_new_null());
    if (!untrusted || (intermediate && sk_X509_push(untrusted.get(), intermediate) <= 0)) return -1;

    const UniqueX509StoreCtx storeCtx(X509_STORE_CTX_new());
    if (!storeCtx || X509_STORE_CTX_init(storeCtx.get(), store.get(), leaf, untrusted.get()) != 1) return -1;
    const int32_t result = CertificateBlocklist::verifyChain(storeCtx.get(), nullptr);
    error = X509_STORE_CTX_get_error(storeCtx.get());
    return result;
}

} // namespace

void TestCertificateBlocklist::testCompromisedIntermediateIsRejected() {
    const auto root = loadCert(emsignRootTlsCaG3Pem);
    const auto intermediate = loadCert(emOvTlsCaG3cPem);
    const auto intermediateKey = loadKey(emOvTlsCaG3cKeyPem);
    CPPUNIT_ASSERT(root && intermediate && intermediateKey);

    // A forged server certificate, signed with the leaked key of the intermediate.
    const UniqueEvpPkey leafKey(EVP_EC_gen("P-256"));
    const auto leaf = makeCert(leafKey.get(), "kdrive.infomaniak.com", intermediate.get(), intermediateKey.get());
    CPPUNIT_ASSERT(leaf);

    int32_t error = X509_V_OK;
    CPPUNIT_ASSERT_EQUAL(int32_t{0}, verify(leaf.get(), root.get(), intermediate.get(), error));
    CPPUNIT_ASSERT_EQUAL(int32_t{X509_V_ERR_CERT_REJECTED}, error);
}

void TestCertificateBlocklist::testUnblockedChainIsAccepted() {
    const UniqueEvpPkey key(EVP_EC_gen("P-256"));
    const auto cert = makeCert(key.get(), "kdrive.infomaniak.com", nullptr, key.get());
    CPPUNIT_ASSERT(cert);

    int32_t error = X509_V_OK;
    CPPUNIT_ASSERT_EQUAL(int32_t{1}, verify(cert.get(), cert.get(), nullptr, error));
    CPPUNIT_ASSERT_EQUAL(int32_t{X509_V_OK}, error);
}

void TestCertificateBlocklist::testUntrustedChainIsStillRejected() {
    // The regular OpenSSL verification must still run: without a trusted root, the chain is refused
    // with the original error, not by the blocklist.
    const UniqueEvpPkey key(EVP_EC_gen("P-256"));
    const auto cert = makeCert(key.get(), "kdrive.infomaniak.com", nullptr, key.get());
    CPPUNIT_ASSERT(cert);

    int32_t error = X509_V_OK;
    CPPUNIT_ASSERT_EQUAL(int32_t{0}, verify(cert.get(), nullptr, nullptr, error));
    CPPUNIT_ASSERT(error != X509_V_OK && error != X509_V_ERR_CERT_REJECTED);
}

} // namespace KDC
