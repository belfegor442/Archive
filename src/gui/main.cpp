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
#include <sstream>

#include "app/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "storage/StoredObjectRepository.h"
#include "storage/ScanRepository.h"
#include "storage/ScanItemRepository.h"
#include "storage/ClassificationRepository.h"
#include "storage/ClassificationRuleRepository.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "storage/UndoRepository.h"
#include "filesystem/StorageManager.h"
#include "services/ImportService.h"
#include "services/VersionService.h"
#include "services/IntegrityService.h"
#include "services/UpdateService.h"
#include "services/ProjectDetector.h"
#include "services/Scanner.h"
#include "services/Classifier.h"
#include "services/OrganizationPlanner.h"
#include "services/OrganizationExecutor.h"

using namespace archive;

enum MenuCmd {
    CMD_IMPORT       = 1001,
    CMD_IMPORT_DIR   = 1002,
    CMD_LIST         = 1003,
    CMD_VERIFY       = 1004,
    CMD_RESTORE      = 1005,
    CMD_TRASH        = 1006,
    CMD_UNTRASH      = 1007,
    CMD_DELETE       = 1008,
    CMD_EXIT         = 1009,
    CMD_ABOUT        = 1010,
    CMD_DETAILS      = 1011,
    CMD_ORG_SCAN     = 1020,
    CMD_ORG_PLAN     = 1021,
    CMD_ORG_EXECUTE  = 1022,
    CMD_ORG_UNDO     = 1023,
    CMD_ORG_APPROVE  = 1024,
};

static HINSTANCE g_hInst = nullptr;
static HWND g_hWnd = nullptr;
static HWND g_hList = nullptr;
static HWND g_hOrgList = nullptr;
static HWND g_hStatus = nullptr;
static HWND g_hTab = nullptr;
static app::AppConfig g_config;
static std::vector<core::ArchiveItem> g_items;

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

struct Svc {
    storage::DatabaseManager db;
    storage::ArchiveItemRepository items;
    storage::CategoryRepository categories;
    storage::TagRepository tags;
    storage::ActivityRepository activities;
    storage::VersionRepository versions;
    storage::StoredObjectRepository stored;
    storage::ScanRepository scans;
    storage::ScanItemRepository scan_items;
    storage::ClassificationRepository classifications;
    storage::ClassificationRuleRepository rules;
    storage::OrgPlanRepository plans;
    storage::OrgMoveRepository moves;
    storage::UndoRepository undos;
    filesystem::StorageManager storage;
    services::ProjectDetector detector;
    Svc() : db(g_config.db_path), items(db), categories(db), tags(db),
            activities(db), versions(db), stored(db),
            scans(db), scan_items(db), classifications(db), rules(db),
            plans(db), moves(db), undos(db),
            storage(g_config.data_dir, g_config.items_dir) { db.initialize(); }
};

static int ActiveTab() {
    TCITEMW tc = {};
    tc.mask = TCIF_TEXT;
    int sel = TabCtrl_GetCurSel(g_hTab);
    if (sel < 0) return 0;
    TabCtrl_GetItem(g_hTab, sel, &tc);
    return sel;
}

static void RefreshArchiveList() {
    ListView_DeleteAllItems(g_hList);
    try {
        Svc s;
        g_items = s.items.find_all();
    } catch (...) { Status(L"Error loading items"); return; }

    for (int i = 0; i < (int)g_items.size(); i++) {
        const auto& it = g_items[i];
        std::wstring nm = ToW(it.name);
        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = (LPWSTR)nm.c_str();
        int idx = ListView_InsertItem(g_hList, &li);

        const wchar_t* types[] = { L"File", L"Folder", L"Project", L"Document" };
        std::wstring tp = types[(int)it.type];
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
    std::wstring m = std::to_wstring(g_items.size()) + L" archived item(s)";
    Status(m.c_str());
}

static void RefreshOrgList(const std::vector<std::wstring>& rows) {
    ListView_DeleteAllItems(g_hOrgList);
    for (int i = 0; i < (int)rows.size(); i++) {
        std::wstring nm = rows[i];
        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = (LPWSTR)nm.c_str();
        ListView_InsertItem(g_hOrgList, &li);
    }
}

static std::wstring PickFolder(HWND h) {
    wchar_t f[MAX_PATH] = L"";
    BROWSEINFOW bi = {};
    bi.hwndOwner = h;
    bi.lpszTitle = L"Select folder to organize";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) { SHGetPathFromIDListW(pidl, f); CoTaskMemFree(pidl); }
    return std::wstring(f);
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

static int SelIdx() { return ListView_GetNextItem(g_hList, -1, LVNI_SELECTED); }

static void DoImport(HWND h, bool dir) {
    std::wstring path = dir ? PickFolder(h) : PickFile(h);
    if (path.empty()) return;
    Status(L"Importing...");
    UpdateWindow(h);
    try {
        Svc s;
        services::ImportService imp(s.db, s.items, s.categories, s.tags,
                                    s.activities, s.versions, s.stored, s.storage, s.detector);
        core::ImportRequest req;
        req.paths.push_back(FromW(path));
        auto res = imp.import(req);
        if (res.has_errors()) {
            MessageBoxW(h, (L"Error: " + ToW(res.errors[0].error)).c_str(), L"Import Error", MB_ICONERROR);
        }
        RefreshArchiveList();
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoDetails(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) { MessageBoxW(h, L"Select an item", L"Info", MB_ICONINFORMATION); return; }
    const auto& it = g_items[i];
    const char* types[] = {"File","Folder","Project","Document"};
    std::wstring info;
    info += L"Name:      " + ToW(it.name) + L"\n";
    info += L"ID:        " + ToW(it.id) + L"\n";
    info += L"Type:      " + ToW(types[(int)it.type]) + L"\n";
    info += L"Status:    " + ToW(core::to_string(it.status)) + L"\n";
    info += L"Size:      " + std::to_wstring(it.size) + L" bytes\n";
    info += L"Version:   " + std::to_wstring(it.current_version) + L"\n";
    info += L"Checksum:  " + ToW(it.checksum) + L"\n";
    info += L"Original:  " + ToW(it.original_path) + L"\n";
    info += L"Storage:   " + ToW(it.storage_path) + L"\n";
    info += L"Created:   " + ToW(it.created_at) + L"\n";
    info += L"Archived:  " + ToW(it.archived_at) + L"\n";
    try {
        Svc s;
        auto vers = s.versions.find_by_item(it.id);
        if (!vers.empty()) {
            info += L"\nVersions (" + std::to_wstring(vers.size()) + L"):\n";
            for (const auto& v : vers)
                info += L"  v" + std::to_wstring(v.version_number) + L"  " + std::to_wstring(v.size) + L" B  " + ToW(v.created_at) + L"\n";
        }
    } catch (...) {}
    MessageBoxW(h, info.c_str(), L"Details", MB_ICONINFORMATION);
}

static void DoVerify(HWND h) {
    Status(L"Verifying...");
    UpdateWindow(h);
    try {
        Svc s;
        services::IntegrityService integ(s.items, s.versions, s.stored, s.activities, s.storage);
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
        Svc s;
        services::VersionService vs(s.db, s.versions, s.items, s.activities, s.stored, s.storage);
        auto lv = s.versions.find_latest(it.id);
        if (!lv) { MessageBoxW(h, L"No versions", L"Error", MB_ICONERROR); return; }
        vs.restore(it.id, lv->id);
        MessageBoxW(h, (L"Restored v" + std::to_wstring(lv->version_number)).c_str(), L"Done", MB_ICONINFORMATION);
        RefreshArchiveList();
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoTrash(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    if (MessageBoxW(h, (L"Trash \"" + ToW(it.name) + L"\"?").c_str(), L"Trash", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    try { Svc s; services::UpdateService u(s.items, s.activities, s.storage); u.move_to_trash(it.id); RefreshArchiveList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static void DoUntrash(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    try { Svc s; services::UpdateService u(s.items, s.activities, s.storage); u.restore_from_trash(it.id); RefreshArchiveList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static void DoDelete(HWND h) {
    int i = SelIdx();
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    if (MessageBoxW(h, (L"PERMANENTLY delete \"" + ToW(it.name) + L"\"?").c_str(), L"Delete", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    try { Svc s; services::UpdateService u(s.items, s.activities, s.storage); u.permanent_delete(it.id); RefreshArchiveList(); }
    catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
}

static std::wstring g_last_scanned_path;
static std::string g_last_scan_id;
static std::string g_last_plan_id;
static std::string g_last_undo_id;

static void DoOrgScan(HWND h) {
    std::wstring path = PickFolder(h);
    if (path.empty()) return;
    g_last_scanned_path = path;
    Status(L"Scanning folder...");
    UpdateWindow(h);
    try {
        Svc s;
        services::Scanner scanner(s.db, s.scans, s.scan_items);
        auto scan = scanner.scan_directory(FromW(path));
        g_last_scan_id = scan.id;

        Status(L"Classifying files...");
        UpdateWindow(h);
        services::Classifier classifier(s.db, s.classifications, s.rules, s.scan_items);
        auto classes = classifier.classify_scan(scan.id, 50);

        std::vector<std::wstring> rows;
        auto items = s.scan_items.find_by_scan(scan.id);
        for (const auto& item : items) {
            auto best = s.classifications.find_best(item.id);
            std::wstring row = ToW(item.filename);
            row += L"  |  " + ToW(best ? best->taxonomy_path : "Unknown");
            row += L"  |  " + ToW(item.extension);
            row += L"  |  " + std::to_wstring(item.size) + L" B";
            rows.push_back(row);
        }
        RefreshOrgList(rows);

        std::wstring msg = L"Scanned " + std::to_wstring(scan.file_count) + L" files, " +
                           std::to_wstring(scan.folder_count) + L" folders, " +
                           std::to_wstring(classes.size()) + L" classified";
        Status(msg.c_str());
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoOrgPlan(HWND h) {
    if (g_last_scan_id.empty()) { MessageBoxW(h, L"Scan a folder first", L"Info", MB_ICONINFORMATION); return; }
    Status(L"Generating plan...");
    UpdateWindow(h);
    try {
        Svc s;
        services::OrganizationPlanner planner(s.db, s.scans, s.scan_items, s.classifications, s.plans, s.moves);
        auto plan = planner.create_plan(g_last_scan_id, FromW(g_last_scanned_path), 50);
        g_last_plan_id = plan.id;
        planner.approve_plan(plan.id);

        auto details = planner.get_moves(plan.id);
        std::vector<std::wstring> rows;
        for (const auto& d : details) {
            std::wstring row = ToW(d.source);
            row += L"  -->  " + ToW(d.destination);
            row += L"  (" + std::to_wstring((int)(d.confidence * 100)) + L"%)";
            rows.push_back(row);
        }
        if (rows.empty()) {
            rows.push_back(L"No moves planned (files already organized)");
        }
        RefreshOrgList(rows);

        std::wstring msg = L"Plan: " + std::to_wstring(plan.moves_planned) + L" moves, " +
                           std::to_wstring(plan.total_files) + L" files, avg confidence " +
                           std::to_wstring((int)(plan.avg_confidence * 100)) + L"%";
        Status(msg.c_str());
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoOrgExecute(HWND h) {
    if (g_last_plan_id.empty()) { MessageBoxW(h, L"Generate a plan first", L"Info", MB_ICONINFORMATION); return; }
    if (MessageBoxW(h, L"Execute this plan? Files will be moved.", L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    Status(L"Executing plan...");
    UpdateWindow(h);
    try {
        Svc s;
        services::OrganizationExecutor executor(s.db, s.plans, s.moves, s.undos);
        auto undo = executor.execute(g_last_plan_id);
        g_last_undo_id = undo.id;

        std::wstring msg = L"Executed! " + std::to_wstring(undo.moves_count) + L" files moved. Undo ID: " + ToW(undo.id);
        Status(msg.c_str());
        MessageBoxW(h, msg.c_str(), L"Organize Complete", MB_ICONINFORMATION);
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void DoOrgUndo(HWND h) {
    if (g_last_undo_id.empty()) { MessageBoxW(h, L"No undo available. Execute a plan first.", L"Info", MB_ICONINFORMATION); return; }
    if (MessageBoxW(h, L"Undo last organize operation?", L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    Status(L"Undoing...");
    UpdateWindow(h);
    try {
        Svc s;
        services::OrganizationExecutor executor(s.db, s.plans, s.moves, s.undos);
        executor.undo(g_last_undo_id);
        g_last_undo_id.clear();
        Status(L"Undo complete. Files restored to original locations.");
        MessageBoxW(h, L"Files restored to original locations.", L"Undo Complete", MB_ICONINFORMATION);
    } catch (const std::exception& e) {
        MessageBoxW(h, (L"Failed: " + ToW(e.what())).c_str(), L"Error", MB_ICONERROR);
    }
}

static void LayoutChildren(HWND h) {
    int w, hh;
    RECT wr; GetClientRect(h, &wr);
    w = wr.right; hh = wr.bottom;

    RECT sr; GetWindowRect(g_hStatus, &sr);
    int sbh = sr.bottom - sr.top;

    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    MoveWindow(g_hTab, 0, 0, w, hh - sbh, TRUE);

    int tab_margin = 4;
    RECT tr; GetWindowRect(g_hTab, &tr);
    TabCtrl_AdjustRect(g_hTab, FALSE, &tr);
    int tx = tr.left; int ty = tr.top; int tw = tr.right - tr.left; int th = tr.bottom - tr.top;

    if (ActiveTab() == 0) {
        ShowWindow(g_hList, SW_SHOW);
        ShowWindow(g_hOrgList, SW_HIDE);
        MoveWindow(g_hList, tx, ty, tw, th, TRUE);
    } else {
        ShowWindow(g_hList, SW_HIDE);
        ShowWindow(g_hOrgList, SW_SHOW);
        MoveWindow(g_hOrgList, tx, ty, tw, th, TRUE);
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, h, (HMENU)3, g_hInst, nullptr);

        TCITEMW tc = {};
        tc.mask = TCIF_TEXT;
        tc.pszText = (LPWSTR)L"Archive";
        TabCtrl_InsertItem(g_hTab, 0, &tc);
        tc.pszText = (LPWSTR)L"Organizer";
        TabCtrl_InsertItem(g_hTab, 1, &tc);

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

        g_hOrgList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, h, (HMENU)4, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_hOrgList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        struct { const wchar_t* n; int w; } ocols[] = {
            {L"File / Source", 300}, {L"Category / Destination", 300}, {L"Extension", 80}, {L"Size", 100}
        };
        for (int i = 0; i < 4; i++) { c.pszText = (LPWSTR)ocols[i].n; c.cx = ocols[i].w; ListView_InsertColumn(g_hOrgList, i, &c); }

        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready — Archive v0.1.0",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, h, (HMENU)2, g_hInst, nullptr);

        RefreshArchiveList();
        break;
    }
    case WM_SIZE: {
        LayoutChildren(h);
        break;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)l;
        if (nm->hwndFrom == g_hTab && nm->code == TCN_SELCHANGE) {
            LayoutChildren(h);
            if (ActiveTab() == 0) {
                RefreshArchiveList();
            }
        }
        if (nm->hwndFrom == g_hList && nm->code == NM_DBLCLK) DoDetails(h);
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
            case CMD_IMPORT:       DoImport(h, false); break;
            case CMD_IMPORT_DIR:   DoImport(h, true); break;
            case CMD_LIST:         RefreshArchiveList(); break;
            case CMD_VERIFY:       DoVerify(h); break;
            case CMD_RESTORE:      DoRestore(h); break;
            case CMD_TRASH:        DoTrash(h); break;
            case CMD_UNTRASH:      DoUntrash(h); break;
            case CMD_DELETE:       DoDelete(h); break;
            case CMD_DETAILS:      DoDetails(h); break;
            case CMD_ORG_SCAN:     DoOrgScan(h); break;
            case CMD_ORG_PLAN:     DoOrgPlan(h); break;
            case CMD_ORG_EXECUTE:  DoOrgExecute(h); break;
            case CMD_ORG_UNDO:     DoOrgUndo(h); break;
            case CMD_ABOUT:        MessageBoxW(h, L"Archive v0.1.0\n\nFile Archiving & Intelligent Organizer\nSHA-256 integrity. Atomic rollback.\n\nScan  >  Classify  >  Plan  >  Execute  >  Undo\n\nMIT License", L"About", MB_ICONINFORMATION); break;
            case CMD_EXIT:         DestroyWindow(h); break;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProcW(h, m, w, l);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nS) {
    g_hInst = hI;
    g_config = app::AppConfig::default_config();
    g_config.ensure_directories();

    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES };
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

    HMENU hOrg = CreatePopupMenu();
    AppendMenuW(hOrg, MF_STRING, CMD_ORG_SCAN, L"&Scan Folder...\tCtrl+Shift+S");
    AppendMenuW(hOrg, MF_STRING, CMD_ORG_PLAN, L"&Generate Plan...\tCtrl+Shift+P");
    AppendMenuW(hOrg, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hOrg, MF_STRING, CMD_ORG_EXECUTE, L"&Execute Plan\tCtrl+Shift+E");
    AppendMenuW(hOrg, MF_STRING, CMD_ORG_UNDO, L"&Undo\tCtrl+Shift+Z");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hOrg, L"&Organize");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, CMD_ABOUT, L"&About");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    g_hWnd = CreateWindowExW(0, L"ArchiveWnd", L"Archive v0.1.0 — Organizer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1020, 620,
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
