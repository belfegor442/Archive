#include "AutoUpdater.hpp"
#include "Version.hpp"
#include "../core/TextUtils.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace monix::updater {

AutoUpdater::AutoUpdater() {
  hSession_ = WinHttpOpen(
    L"Monix AutoUpdater/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS,
    0
  );
}

AutoUpdater::~AutoUpdater() {
  CancelPendingCheck();
  if (hSession_) {
    WinHttpCloseHandle(hSession_);
    hSession_ = nullptr;
  }
}

void AutoUpdater::CancelPendingCheck() {
  if (worker_.joinable()) {
    worker_.detach();
  }
  checking_.store(false);
}

bool AutoUpdater::HttpsGet(const std::wstring& url, std::string& responseBody) {
  if (!hSession_) return false;

  URL_COMPONENTS urlComp{};
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwSchemeLength = 1;
  urlComp.dwHostNameLength = 1;
  urlComp.dwUrlPathLength = 1;
  urlComp.dwExtraInfoLength = 1;
  urlComp.nScheme = INTERNET_SCHEME_HTTPS;

  if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &urlComp)) {
    return false;
  }

  std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
  std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
  if (urlComp.lpszExtraInfo && urlComp.dwExtraInfoLength > 0) {
    path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
  }

  HINTERNET hConnect = WinHttpConnect(hSession_, host.c_str(), urlComp.nPort, 0);
  if (!hConnect) return false;

  HINTERNET hRequest = WinHttpOpenRequest(
    hConnect,
    L"GET",
    path.c_str(),
    nullptr,
    WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    WINHTTP_FLAG_SECURE
  );
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    return false;
  }

  DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                        SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
  WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));

  BOOL sent = WinHttpSendRequest(
    hRequest,
    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
    WINHTTP_NO_REQUEST_DATA, 0,
    0, 0
  );

  if (!sent) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return false;
  }

  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return false;
  }

  responseBody.clear();
  DWORD bytesAvailable = 0;
  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
    std::vector<char> buffer(bytesAvailable + 1, 0);
    DWORD bytesRead = 0;
    if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
      responseBody.append(buffer.data(), bytesRead);
    }
    bytesAvailable = 0;
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  return !responseBody.empty();
}

void AutoUpdater::StripQuotes(std::string& s) {
  while (!s.empty() && (s.front() == '"' || s.front() == '\'' || s.front() == ' ')) {
    s.erase(s.begin());
  }
  while (!s.empty() && (s.back() == '"' || s.back() == '\'' || s.back() == ' ')) {
    s.pop_back();
  }
}

bool AutoUpdater::ParseJsonString(const std::string& json, const std::string& key, std::string& value) {
  std::string searchKey = "\"" + key + "\"";
  auto pos = json.find(searchKey);
  if (pos == std::string::npos) return false;

  auto colonPos = json.find(':', pos + searchKey.size());
  if (colonPos == std::string::npos) return false;

  auto valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
  if (valueStart == std::string::npos) return false;

  if (json[valueStart] == '"') {
    auto valueEnd = json.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) return false;
    value = json.substr(valueStart + 1, valueEnd - valueStart - 1);
  } else {
    auto valueEnd = json.find_first_of(",}\n", valueStart);
    if (valueEnd == std::string::npos) valueEnd = json.size();
    value = json.substr(valueStart, valueEnd - valueStart);
  }
  return true;
}

bool AutoUpdater::CompareVersions(const std::wstring& remote, const std::wstring& local) {
  auto parse = [](const std::wstring& v) -> std::tuple<int, int, int> {
    std::wstring cleaned = v;
    if (!cleaned.empty() && (cleaned[0] == L'v' || cleaned[0] == L'V')) {
      cleaned = cleaned.substr(1);
    }
    int major = 0, minor = 0, patch = 0;
    std::wistringstream iss(cleaned);
    wchar_t dot;
    iss >> major;
    if (iss.peek() == L'.') { iss >> dot >> minor; }
    if (iss.peek() == L'.') { iss >> dot >> patch; }
    return {major, minor, patch};
  };

  auto [rm, rp, rpa] = parse(remote);
  auto [lm, lp, lpa] = parse(local);

  if (rm != lm) return rm > lm;
  if (rp != lp) return rp > lp;
  return rpa > lpa;
}

void AutoUpdater::CheckForUpdateAsync(Callback callback) {
  if (checking_.load()) return;

  CancelPendingCheck();
  checking_.store(true);

  worker_ = std::thread([this, cb = std::move(callback)]() {
    UpdateInfo info{};
    info.currentVersion = MONIX_VERSION_TAG_W;

    std::string response;
    if (HttpsGet(MONIX_GITHUB_API_URL_W, response)) {
      std::string tagName, body, htmlUrl;

      if (ParseJsonString(response, "tag_name", tagName)) {
        StripQuotes(tagName);
        info.tagName = monix::Utf8ToWide(tagName);
        info.latestVersion = info.tagName;
      }

      if (ParseJsonString(response, "body", body)) {
        StripQuotes(body);
        info.body = monix::Utf8ToWide(body);
      }

      if (ParseJsonString(response, "html_url", htmlUrl)) {
        StripQuotes(htmlUrl);
        info.htmlUrl = monix::Utf8ToWide(htmlUrl);
      }

      auto assetsPos = response.find("\"assets\"");
      if (assetsPos != std::string::npos) {
        auto bracketStart = response.find('[', assetsPos);
        if (bracketStart != std::string::npos) {
          auto bracketEnd = response.find(']', bracketStart);
          if (bracketEnd != std::string::npos) {
            std::string assetsSection = response.substr(bracketStart, bracketEnd - bracketStart + 1);
            std::string nameKey = "\"name\"";
            auto namePos = assetsSection.find(nameKey);
            if (namePos != std::string::npos) {
              std::string assetName;
              if (ParseJsonString(assetsSection.substr(namePos), "name", assetName)) {
                StripQuotes(assetName);
                if (assetName.find(".exe") != std::string::npos) {
                  std::string browserUrl;
                  if (ParseJsonString(assetsSection.substr(namePos), "browser_download_url", browserUrl)) {
                    StripQuotes(browserUrl);
                    info.exeDownloadUrl = monix::Utf8ToWide(browserUrl);
                  }
                }
              }
            }
          }
        }
      }

      if (!info.tagName.empty()) {
        info.available = CompareVersions(info.tagName, info.currentVersion);
      }
    }

    if (info.htmlUrl.empty()) {
      info.htmlUrl = MONIX_GITHUB_RELEASES_URL_W;
    }

    checking_.store(false);
    if (cb) cb(info);
  });
}

bool AutoUpdater::CheckForUpdateSync(UpdateInfo& info) {
  info.currentVersion = MONIX_VERSION_TAG_W;

  std::string response;
  if (!HttpsGet(MONIX_GITHUB_API_URL_W, response)) {
    info.htmlUrl = MONIX_GITHUB_RELEASES_URL_W;
    return false;
  }

  std::string tagName, body, htmlUrl;
  if (ParseJsonString(response, "tag_name", tagName)) {
    StripQuotes(tagName);
    info.tagName = monix::Utf8ToWide(tagName);
    info.latestVersion = info.tagName;
  }
  if (ParseJsonString(response, "body", body)) {
    StripQuotes(body);
    info.body = monix::Utf8ToWide(body);
  }
  if (ParseJsonString(response, "html_url", htmlUrl)) {
    StripQuotes(htmlUrl);
    info.htmlUrl = monix::Utf8ToWide(htmlUrl);
  }

  if (info.htmlUrl.empty()) {
    info.htmlUrl = MONIX_GITHUB_RELEASES_URL_W;
  }

  if (!info.tagName.empty()) {
    info.available = CompareVersions(info.tagName, info.currentVersion);
  }

  return info.available;
}

} // namespace monix::updater
