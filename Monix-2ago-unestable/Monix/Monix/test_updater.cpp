#include "updater/Version.hpp"
#include "updater/AutoUpdater.hpp"
#include "core/TextUtils.hpp"
#include <cstdio>

int main() {
    wprintf(L"Monix AutoUpdater Test\n");
    wprintf(L"Current version: %S\n", MONIX_VERSION);
    wprintf(L"Checking %S ...\n", MONIX_GITHUB_API_URL);

    monix::updater::AutoUpdater updater;

    wprintf(L"Sending HTTPS GET request...\n");
    monix::updater::UpdateInfo info;
    bool ok = updater.CheckForUpdateSync(info);

    wprintf(L"\n--- Update Check Result ---\n");
    wprintf(L"Available:  %s\n", info.available ? L"YES" : L"NO");
    wprintf(L"Current:    %s\n", info.currentVersion.c_str());
    wprintf(L"Latest:     %s\n", info.latestVersion.c_str());
    wprintf(L"Tag:        %s\n", info.tagName.c_str());
    wprintf(L"URL:        %s\n", info.htmlUrl.c_str());
    wprintf(L"Download:   %s\n", info.exeDownloadUrl.c_str());
    wprintf(L"Notes:      %s\n", info.body.c_str());
    wprintf(L"HttpGet ok: %s\n", ok ? L"true" : L"false");

    return info.available ? 1 : 0;
}
