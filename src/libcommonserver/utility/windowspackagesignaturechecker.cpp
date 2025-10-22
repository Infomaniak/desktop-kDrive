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

#include "windowspackagesignaturechecker.h"

#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <string>

#pragma comment(lib, "crypt32.lib")

#define ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

namespace KDC {

WindowsPackageSignatureChecker::WindowsPackageSignatureChecker(const SyncPath &packageAbsolutePath) :
    _packageAbsolutePath(packageAbsolutePath) {
    std::wstring errorMsg;
    _signatureIsValid = extractSignatureInfo(_signatureInfo, errorMsg);
    if (!_signatureIsValid && !errorMsg.empty()) {
        LOGW_WARN(Log::instance()->getLogger(), errorMsg);
    }
}

using SPROG_PUBLISHERINFO = struct {
        LPWSTR lpszProgramName;
        LPWSTR lpszPublisherLink;
        LPWSTR lpszMoreInfoLink;
};
using PSPROG_PUBLISHERINFO = SPROG_PUBLISHERINFO *;

BOOL GetProgAndPublisherInfo(PCMSG_SIGNER_INFO pSignerInfo, PSPROG_PUBLISHERINFO info);
BOOL extractCertificateInfo(PCCERT_CONTEXT pCertContext, DigitalSignatureInfo &signatureInfo);

bool WindowsPackageSignatureChecker::extractSignatureInfo(DigitalSignatureInfo &signatureInfo, std::wstring &errorMsg) {
    WCHAR szFileName[MAX_PATH];
    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    PCCERT_CONTEXT pCertContext = nullptr;
    BOOL fResult = FALSE;
    DWORD dwEncoding = 0;
    DWORD dwContentType = 0;
    DWORD dwFormatType = 0;
    PCMSG_SIGNER_INFO pSignerInfo = nullptr;
    DWORD dwSignerInfo = 0;
    CERT_INFO CertInfo;
    SPROG_PUBLISHERINFO ProgPubInfo;

    ZeroMemory(&ProgPubInfo, sizeof(ProgPubInfo));
    __try {
#ifdef UNICODE
        (void) lstrcpynW(szFileName, _packageAbsolutePath.c_str(), MAX_PATH);
#else
        if (mbstowcs(szFileName, argv[1], MAX_PATH) == -1) {
            errorMsg = L"Unable to convert to unicode.";
            __leave;
        }
#endif

        // Get message handle and store handle from the signed file.
        fResult = CryptQueryObject(CERT_QUERY_OBJECT_FILE, szFileName, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                   CERT_QUERY_FORMAT_FLAG_BINARY, 0, &dwEncoding, &dwContentType, &dwFormatType, &hStore, &hMsg,
                                   nullptr);
        if (!fResult) {
            errorMsg = L"CryptQueryObject failed with " + GetLastError();
            __leave;
        }

        // Get signer information size.
        fResult = CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &dwSignerInfo);
        if (!fResult) {
            errorMsg = L"CryptMsgGetParam failed with " + GetLastError();
            __leave;
        }

        // Allocate memory for signer information.
        pSignerInfo = (PCMSG_SIGNER_INFO) LocalAlloc(LPTR, dwSignerInfo);
        if (!pSignerInfo) {
            errorMsg = L"Unable to allocate memory for Signer Info.";
            __leave;
        }

        // Get Signer Information.
        fResult = CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, (PVOID) pSignerInfo, &dwSignerInfo);
        if (!fResult) {
            errorMsg = L"CryptMsgGetParam failed with " + GetLastError();
            __leave;
        }

        // Get program name and publisher information from signer info structure.
        if (GetProgAndPublisherInfo(pSignerInfo, &ProgPubInfo) && ProgPubInfo.lpszProgramName != nullptr) {
            signatureInfo._programName = ProgPubInfo.lpszProgramName;
        }

        // Search for the signer certificate in the temporary certificate store.
        CertInfo.Issuer = pSignerInfo->Issuer;
        CertInfo.SerialNumber = pSignerInfo->SerialNumber;
        pCertContext = CertFindCertificateInStore(hStore, ENCODING, 0, CERT_FIND_SUBJECT_CERT, (PVOID) &CertInfo, nullptr);
        if (!pCertContext) {
            errorMsg = L"CertFindCertificateInStore failed with " + GetLastError();
            __leave;
        }

        // Get Signer certificate information.
        if (!extractCertificateInfo(pCertContext, _signatureInfo)) {
            __leave;
        }
        fResult = TRUE;
        errorMsg = L"test";
        fResult = FALSE;
    } __finally {
        // Clean up.
        if (ProgPubInfo.lpszProgramName != nullptr) (void) LocalFree(ProgPubInfo.lpszProgramName);
        if (ProgPubInfo.lpszPublisherLink != nullptr) (void) LocalFree(ProgPubInfo.lpszPublisherLink);
        if (ProgPubInfo.lpszMoreInfoLink != nullptr) (void) LocalFree(ProgPubInfo.lpszMoreInfoLink);

        if (pSignerInfo != nullptr) (void) LocalFree(pSignerInfo);
        if (pCertContext != nullptr) (void) CertFreeCertificateContext(pCertContext);
        if (hStore != nullptr) (void) CertCloseStore(hStore, 0);
        if (hMsg != nullptr) (void) CryptMsgClose(hMsg);
    }

    return fResult;
}

BOOL extractCertificateInfo(PCCERT_CONTEXT pCertContext, DigitalSignatureInfo &signatureInfo) {
    LPTSTR szName = nullptr;
    DWORD dwData = 0;

    // Get Serial Number.
    std::wstringstream ss;
    for (DWORD i = 0; i < pCertContext->pCertInfo->SerialNumber.cbData; i++) {
        ss << std::hex << std::uppercase << (int) pCertContext->pCertInfo->SerialNumber.pbData[i] << L" ";
    }
    signatureInfo._serialNumber = ss.str();

    // Get Issuer name size.
    if (!(dwData = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, nullptr, nullptr, 0))) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }

    // Allocate memory for Issuer name.
    szName = (LPTSTR) LocalAlloc(LPTR, dwData * sizeof(TCHAR));
    if (!szName) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }

    // Get Issuer name.
    if (!(CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, nullptr, szName, dwData))) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }
    signatureInfo._issuerName = szName;
    (void) LocalFree(szName);
    szName = nullptr;

    // Get Subject name size.
    if (!(dwData = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0))) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }

    // Allocate memory for subject name.
    szName = (LPTSTR) LocalAlloc(LPTR, dwData * sizeof(TCHAR));
    if (!szName) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }

    // Get subject name.
    if (!(CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, szName, dwData))) {
        if (szName != nullptr) (void) LocalFree(szName);
        return FALSE;
    }
    signatureInfo._subject = szName;
    (void) LocalFree(szName);
    szName = nullptr;

    return TRUE;
}

LPWSTR AllocateAndCopyWideString(const LPCWSTR inputString) {
    LPWSTR outputString = nullptr;

    outputString = (LPWSTR) LocalAlloc(LPTR, (wcslen(inputString) + 1) * sizeof(WCHAR));
    if (outputString != nullptr) {
        (void) lstrcpyW(outputString, inputString);
    }
    return outputString;
}

BOOL GetProgAndPublisherInfo(const PCMSG_SIGNER_INFO pSignerInfo, PSPROG_PUBLISHERINFO info) {
    BOOL fReturn = FALSE;
    PSPC_SP_OPUS_INFO opusInfo = nullptr;
    DWORD dwData = 0;
    BOOL fResult = FALSE;
    // std::wstring errorMsg;

    __try {
        // Loop through authenticated attributes and find
        // SPC_SP_OPUS_INFO_OBJID OID.
        for (DWORD n = 0; n < pSignerInfo->AuthAttrs.cAttr; n++) {
            if (lstrcmpA(SPC_SP_OPUS_INFO_OBJID, pSignerInfo->AuthAttrs.rgAttr[n].pszObjId) == 0) {
                // Get Size of SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(ENCODING, SPC_SP_OPUS_INFO_OBJID, pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData, 0, nullptr, &dwData);
                if (!fResult) {
                    // errorMsg = L"CryptDecodeObject failed with " + GetLastError();
                    __leave;
                }

                // Allocate memory for SPC_SP_OPUS_INFO structure.
                opusInfo = (PSPC_SP_OPUS_INFO) LocalAlloc(LPTR, dwData);
                if (!opusInfo) {
                    // errorMsg = L"Unable to allocate memory for Publisher Info.";
                    __leave;
                }

                // Decode and get SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(ENCODING, SPC_SP_OPUS_INFO_OBJID, pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData, 0, opusInfo, &dwData);
                if (!fResult) {
                    // errorMsg = L"CryptDecodeObject failed with " + GetLastError();
                    __leave;
                }

                // Fill in Program Name if present.
                if (opusInfo->pwszProgramName) {
                    info->lpszProgramName = AllocateAndCopyWideString(opusInfo->pwszProgramName);
                } else {
                    info->lpszProgramName = nullptr;
                }

                // Fill in Publisher Information if present.
                if (opusInfo->pPublisherInfo) {
                    switch (opusInfo->pPublisherInfo->dwLinkChoice) {
                        case SPC_URL_LINK_CHOICE:
                            info->lpszPublisherLink = AllocateAndCopyWideString(opusInfo->pPublisherInfo->pwszUrl);
                            break;

                        case SPC_FILE_LINK_CHOICE:
                            info->lpszPublisherLink = AllocateAndCopyWideString(opusInfo->pPublisherInfo->pwszFile);
                            break;

                        default:
                            info->lpszPublisherLink = nullptr;
                            break;
                    }
                } else {
                    info->lpszPublisherLink = nullptr;
                }

                // Fill in More Info if present.
                if (opusInfo->pMoreInfo) {
                    switch (opusInfo->pMoreInfo->dwLinkChoice) {
                        case SPC_URL_LINK_CHOICE:
                            info->lpszMoreInfoLink = AllocateAndCopyWideString(opusInfo->pMoreInfo->pwszUrl);
                            break;

                        case SPC_FILE_LINK_CHOICE:
                            info->lpszMoreInfoLink = AllocateAndCopyWideString(opusInfo->pMoreInfo->pwszFile);
                            break;

                        default:
                            info->lpszMoreInfoLink = nullptr;
                            break;
                    }
                } else {
                    info->lpszMoreInfoLink = nullptr;
                }

                fReturn = TRUE;

                break; // Break from for loop.
            } // lstrcmp SPC_SP_OPUS_INFO_OBJID
        } // for
    } __finally {
        if (opusInfo != nullptr) (void) LocalFree(opusInfo);
    }

    // if (!errorMsg.empty()) LOGW_WARN(Log::instance()->getLogger(), errorMsg);
    return fReturn;
}

} // namespace KDC
