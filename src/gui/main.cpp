#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>

#include "app/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "storage/StoredObjectRepository.h"
#include "filesystem/StorageManager.h"
#include "services/ImportService.h"
#include "services/VersionService.h"
#include "services/IntegrityService.h"
#include "services/UpdateService.h"
#include "services/ProjectDetector.h"
#include <memory>

using namespace archive;

static constexpr const wchar_t* APP_TITLE = L"Archive v0.1.0";
static constexpr const wchar_t* APP_ABOUT = L"Archive v0.1.0\n\nFile archiving & version management.\nSHA-256 integrity. Atomic rollback.\n\nMIT License";

enum MenuCmd {
    CMD_IMPORT     = 1001,
    CMD_IMPORT_DIR = 1002,
    CMD_LIST       = 1003,
    CMD_VERIFY     = 1004,
    CMD_RESTORE    = 1005,
    CMD_TRASH      = 1006,
    CMD_UNTRASH    = 1007,
    CMD_DELETE     = 1008,
    CMD_EXIT       = 1009,
    CMD_ABOUT      = 1010,
    CMD_DETAILS    = 1011,
};

static HINSTANCE g_hInst = nullptr;
static HWND g_hWnd = nullptr;
static HWND g_hList = nullptr;
static HWND g_hStatus = nullptr;
static app::AppConfig g_config;
static std::vector<core::ArchiveItem> g_items;

struct Svc {
    storage::DatabaseManager db;
    storage::ArchiveItemRepository items;
    storage::CategoryRepository categories;
    storage::TagRepository tags;
    storage::ActivityRepository activities;
    storage::VersionRepository versions;
    storage::StoredObjectRepository stored;
    filesystem::StorageManager storage;
    services::ProjectDetector detector;
    Svc() : db(g_config.db_path), items(db), categories(db), tags(db),
            activities(db), versions(db), stored(db),
            storage(g_config.data_dir, g_config.items_dir) { db.initialize(); }
};

static std::unique_ptr<Svc> g_svc;

static std::wstring ToW(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &ws[0], len);
    return ws;
}

static std::string FromW(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], len, nullptr, nullptr);
    return s;
}

static void Status(const wchar_t* txt) {
    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)txt);
}

static void RefreshList() {
    ListView_DeleteAllItems(g_hList);
    try {
        g_items = g_svc->items.find_all();
    } catch (...) { Status(L"Error loading items"); return; }

    for (int i = 0; i < (int)g_items.size(); i++) {
        const auto& it = g_items[i];
        std::wstring nm = ToW(it.name);
        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = (LPWSTR)nm.c_str();
        int idx = ListView_InsertItem(g_hList, &li);

        std::wstring tp = ToW(core::to_string(it.type));
        std::wstring st = ToW(core::to_string(it.status));
        std::wstring vr = L"v" + std::to_wstring(it.current_version);
        std::wstring sz = std::to_wstring(it.size) + L" B";
        std::wstring id = ToW(it.id);

        ListView_SetItemText(g_hList, idx, 1, (LPWSTR)tp.c_str());
        ListView_SetItemText(g_hList, idx, 2, (LPWSTR)st.c_str());
        ListView_SetItemText(g_hList, idx, 3, (LPWSTR)vr.c_str());
        ListView_SetItemText(g_hList, idx, 4, (LPWSTR)sz.c_str());
        ListView_SetItemText(g_hList, idx, 5, (LPWSTR)id.c_str());
    }
    std::wstring m = std::to_wstring(g_items.size()) + L" item(s)";
    Status(m.c_str());
}

static std::wstring PickFile(HWND h) {
    wchar_t f[MAX_PATH] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = h;
    o.lpstrFile = f;
    o.nMaxFile = MAX_PATH;
    o.lpstrTitle = L"Select file";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    o.lpstrFilter = L"All files (*.*)\0*.*\0";
    return GetOpenFileNameW(&o) ? std::wstring(f) : L"";
}

static std::wstring PickFolder(HWND h) {
    wchar_t f[MAX_PATH] = L"";
    BROWSEINFOW bi = {};
    bi.hwndOwner = h;
    bi.lpszTitle = L"Select folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) { SHGetPathFromIDListW(pidl, f); CoTaskMemFree(pidl); }
    return std::wstring(f);
}

static void DoImport(HWND h, bool dir) {
    std::wstring path = dir ? PickFolder(h) : PickFile(h);
    if (path.empty()) return;
    Status(L"Importing...");
    UpdateWindow(h);
    try {
        services::ImportService imp(g_svc->db, g_svc->items, g_svc->categories, g_svc->tags,
                                    g_svc->activities, g_svc->versions, g_svc->stored, g_svc->storage, g_svc->detector);
        core::ImportRequest req;
        req.paths.push_back(FromW(path));
        auto res = imp.import(req);
        if (res.has_errors()) {
            MessageBoxW(h, (L"Error: " + ToW(res.errors[0].error)).c_str(), L"Import Error", MB_ICONERROR);
        }
        RefreshList();
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static int SelIdx() { return ListView_GetNextItem(g_hList, -1, LVNI_SELECTED); }

static void DoDetails(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) { MessageBoxW(h, L"Select an item", L"Info", MB_ICONINFORMATION); return; }
    const auto& it = g_items[i];
    std::wstring info;
    info += L"Name:      " + ToW(it.name) + L"\n";
    info += L"ID:        " + ToW(it.id) + L"\n";
    info += L"Type:      " + ToW(core::to_string(it.type)) + L"\n";
    info += L"Status:    " + ToW(core::to_string(it.status)) + L"\n";
    info += L"Size:      " + std::to_wstring(it.size) + L" bytes\n";
    info += L"Version:   " + std::to_wstring(it.current_version) + L"\n";
    info += L"Checksum:  " + ToW(it.checksum) + L"\n";
    info += L"Original:  " + ToW(it.original_path) + L"\n";
    info += L"Storage:   " + ToW(it.storage_path) + L"\n";
    info += L"Created:   " + ToW(it.created_at) + L"\n";
    info += L"Archived:  " + ToW(it.archived_at) + L"\n";
    try {
        auto vers = g_svc->versions.find_by_item(it.id);
        if (!vers.empty()) {
            info += L"\nVersions (" + std::to_wstring(vers.size()) + L"):\n";
            for (const auto& v : vers)
                info += L"  v" + std::to_wstring(v.version_number) + L"  " + std::to_wstring(v.size) + L" B  " + ToW(v.created_at) + L"\n";
        }
    } catch (const std::exception& e) { info += L"\nError loading versions: " + ToW(e.what()); }
    MessageBoxW(h, info.c_str(), L"Details", MB_ICONINFORMATION);
}

static void DoVerify(HWND h) {
    Status(L"Verifying...");
    UpdateWindow(h);
    try {
        services::IntegrityService integ(g_svc->items, g_svc->versions, g_svc->stored, g_svc->activities, g_svc->storage);
        auto r = integ.verify_all();
        std::wstring m = L"Valid: " + std::to_wstring(r.valid_count);
        if (r.modified_count) m += L", Modified: " + std::to_wstring(r.modified_count);
        if (r.missing_count) m += L", Missing: " + std::to_wstring(r.missing_count);
        MessageBoxW(h, m.c_str(), L"Verify", r.all_valid() ? MB_ICONINFORMATION : MB_ICONWARNING);
        Status(m.c_str());
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoRestore(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) { MessageBoxW(h, L"Select an item", L"Info", MB_ICONINFORMATION); return; }
    const auto& it = g_items[i];
    if (MessageBoxW(h, (L"Restore \"" + ToW(it.name) + L"\"?").c_str(), L"Restore", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    Status(L"Restoring...");
    UpdateWindow(h);
    try {
        services::VersionService vs(g_svc->db, g_svc->versions, g_svc->items, g_svc->activities, g_svc->stored, g_svc->storage);
        auto lv = g_svc->versions.find_latest(it.id);
        if (!lv) { MessageBoxW(h, L"No versions", L"Error", MB_ICONERROR); return; }
        vs.restore(it.id, lv->id);
        MessageBoxW(h, (L"Restored v" + std::to_wstring(lv->version_number)).c_str(), L"Done", MB_ICONINFORMATION);
        RefreshList();
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoTrash(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    if (MessageBoxW(h, (L"Trash \"" + ToW(it.name) + L"\"?").c_str(), L"Trash", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    try { services::UpdateService u(g_svc->db, g_svc->items, g_svc->activities, g_svc->storage); u.move_to_trash(it.id); RefreshList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static void DoUntrash(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    try { services::UpdateService u(g_svc->db, g_svc->items, g_svc->activities, g_svc->storage); u.restore_from_trash(it.id); RefreshList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static void DoDelete(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    if (MessageBoxW(h, (L"PERMANENTLY delete \"" + ToW(it.name) + L"\"?").c_str(), L"Delete", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    try { services::UpdateService u(g_svc->db, g_svc->items, g_svc->activities, g_svc->storage); u.permanent_delete(it.id); RefreshList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, h, (HMENU)1, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        struct { const wchar_t* n; int w; } cols[] = {
            {L"Name", 200}, {L"Type", 80}, {L"Status", 80}, {L"Version", 70}, {L"Size", 100}, {L"ID", 280}
        };
        LVCOLUMNW c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.fmt = LVCFMT_LEFT;
        for (int i = 0; i < 6; i++) { c.pszText = (LPWSTR)cols[i].n; c.cx = cols[i].w; ListView_InsertColumn(g_hList, i, &c); }

        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, h, (HMENU)2, g_hInst, nullptr);
        RefreshList();
        break;
    }
    case WM_SIZE: {
        int ww = LOWORD(l), hh = HIWORD(l);
        SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT sr; GetWindowRect(g_hStatus, &sr);
        MoveWindow(g_hList, 0, 0, ww, hh - (sr.bottom - sr.top), TRUE);
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
            case CMD_IMPORT:     DoImport(h, false); break;
            case CMD_IMPORT_DIR: DoImport(h, true); break;
            case CMD_LIST:       RefreshList(); break;
            case CMD_VERIFY:     DoVerify(h); break;
            case CMD_RESTORE:    DoRestore(h); break;
            case CMD_TRASH:      DoTrash(h); break;
            case CMD_UNTRASH:    DoUntrash(h); break;
            case CMD_DELETE:     DoDelete(h); break;
            case CMD_DETAILS:    DoDetails(h); break;
            case CMD_ABOUT:      MessageBoxW(h, APP_ABOUT, L"About", MB_ICONINFORMATION); break;
            case CMD_EXIT:       DestroyWindow(h); break;
        }
        break;
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)l;
        if (nm->hwndFrom == g_hList && nm->code == NM_DBLCLK) DoDetails(h);
        break;
    }
    case WM_DESTROY: g_svc.reset(); PostQuitMessage(0); break;
    default: return DefWindowProcW(h, m, w, l);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nS) {
    g_hInst = hI;
    g_config = app::AppConfig::default_config();
    g_config.ensure_directories();
    g_svc = std::make_unique<Svc>();

    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&ic);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ArchiveWnd";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HMENU hBar = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, CMD_IMPORT, L"Import &File...\tCtrl+I");
    AppendMenuW(hFile, MF_STRING, CMD_IMPORT_DIR, L"Import &Folder...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, CMD_EXIT, L"E&xit");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hAct = CreatePopupMenu();
    AppendMenuW(hAct, MF_STRING, CMD_LIST, L"&Refresh\tF5");
    AppendMenuW(hAct, MF_STRING, CMD_DETAILS, L"&Details\tEnter");
    AppendMenuW(hAct, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAct, MF_STRING, CMD_RESTORE, L"&Restore");
    AppendMenuW(hAct, MF_STRING, CMD_TRASH, L"&Trash\tDel");
    AppendMenuW(hAct, MF_STRING, CMD_UNTRASH, L"Untr&ash");
    AppendMenuW(hAct, MF_STRING, CMD_DELETE, L"Delete &Permanently");
    AppendMenuW(hAct, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAct, MF_STRING, CMD_VERIFY, L"&Verify All");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hAct, L"&Actions");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, CMD_ABOUT, L"&About");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    g_hWnd = CreateWindowExW(0, L"ArchiveWnd", APP_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 920, 560,
        nullptr, hBar, hI, nullptr);

    ShowWindow(g_hWnd, nS);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
