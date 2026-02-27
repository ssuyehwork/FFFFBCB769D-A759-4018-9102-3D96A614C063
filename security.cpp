#include "security.h"
#include <wincrypt.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <vector>

namespace Security {

std::wstring HashPassword(const std::wstring& password) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
    pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);
    BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0);
    BCryptHashData(hHash, (PBYTE)password.c_str(), (ULONG)password.length() * sizeof(wchar_t), 0);
    BCryptFinishHash(hHash, pbHash, cbHash, 0);

    std::wstring result;
    for (DWORD i = 0; i < cbHash; i++) {
        wchar_t buf[3];
        swprintf(buf, 3, L"%02x", pbHash[i]);
        result += buf;
    }

    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (pbHashObject) HeapFree(GetProcessHeap(), 0, pbHashObject);
    if (pbHash) HeapFree(GetProcessHeap(), 0, pbHash);

    return result;
}

std::wstring GetCredPath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    wcscat(path, L"\\FocusLock");
    CreateDirectoryW(path, NULL);
    wcscat(path, L"\\credentials.dat");
    return path;
}

bool SavePassword(const std::wstring& password) {
    std::wstring hashed = HashPassword(password);
    DATA_BLOB dataIn, dataOut;
    dataIn.pbData = (BYTE*)hashed.c_str();
    dataIn.cbData = (DWORD)(hashed.length() * sizeof(wchar_t));

    if (CryptProtectData(&dataIn, L"FocusLock", NULL, NULL, NULL, 0, &dataOut)) {
        FILE* fp;
        if (_wfopen_s(&fp, GetCredPath().c_str(), L"wb") == 0) {
            fwrite(dataOut.pbData, 1, dataOut.cbData, fp);
            fclose(fp);
            LocalFree(dataOut.pbData);
            return true;
        }
        LocalFree(dataOut.pbData);
    }
    return false;
}

bool VerifyPassword(const std::wstring& input) {
    FILE* fp;
    if (_wfopen_s(&fp, GetCredPath().c_str(), L"rb") != 0) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<BYTE> buffer(size);
    fread(buffer.data(), 1, size, fp);
    fclose(fp);

    DATA_BLOB dataIn, dataOut;
    dataIn.pbData = buffer.data();
    dataIn.cbData = (DWORD)size;

    if (CryptUnprotectData(&dataIn, NULL, NULL, NULL, NULL, 0, &dataOut)) {
        std::wstring storedHashed((wchar_t*)dataOut.pbData, dataOut.cbData / sizeof(wchar_t));
        std::wstring inputHashed = HashPassword(input);
        LocalFree(dataOut.pbData);

        // Constant-time comparison
        if (storedHashed.length() != inputHashed.length()) return false;
        int diff = 0;
        for (size_t i = 0; i < storedHashed.length(); i++) {
            diff |= (storedHashed[i] ^ inputHashed[i]);
        }
        return diff == 0;
    }
    return false;
}

}
