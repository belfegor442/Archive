#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <functional>

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
#include "storage/TaxonomyRepository.h"
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
#include "core/types/AnalysisResult.h"
#include "core/types/OrgPlanSummary.h"
#include "core/types/MoveDetail.h"
#include "core/models/OrgPlan.h"
#include "core/models/UndoRecord.h"
#include "core/enums/PlanStatus.h"
#include "core/enums/MoveStatus.h"
#include "core/enums/UndoStatus.h"

using namespace archive;

// ============================================================
// Theme constants (COLORREF = 0x00BBGGRR)
// ============================================================
static constexpr COLORREF CLR_BG        = 0x2E1E1E;
static constexpr COLORREF CLR_BG_LIGHT  = 0x3C2A2A;
static constexpr COLORREF CLR_BG_INPUT  = 0x4A3333;
static constexpr COLORREF CLR_SURFACE   = 0x503838;
static constexpr COLORREF CLR_BORDER    = 0x6A4A4A;
static constexpr COLORREF CLR_TEXT      = 0xF0E0E0;
static constexpr COLORREF CLR_TEXT_DIM  = 0xAA8888;
static constexpr COLORREF CLR_ACCENT    = 0xFF8C6C;
static constexpr COLORREF CLR_ROW_ALT   = 0x382424;

static HBRUSH hbrBg = nullptr;
static HBRUSH hbrBgLight = nullptr;
static HBRUSH hbrBgInput = nullptr;
static HBRUSH hbrSurface = nullptr;
static HBRUSH hbrBorder = nullptr;

static void InitBrushes() {
    hbrBg       = CreateSolidBrush(CLR_BG);
    hbrBgLight  = CreateSolidBrush(CLR_BG_LIGHT);
    hbrBgInput  = CreateSolidBrush(CLR_BG_INPUT);
    hbrSurface  = CreateSolidBrush(CLR_SURFACE);
    hbrBorder   = CreateSolidBrush(CLR_BORDER);
}

// ============================================================
// App title
// ============================================================
static constexpr const wchar_t* APP_TITLE = L"Archive";
static constexpr const wchar_t* APP_ABOUT = L"Archive v1.0.0\n\nIntelligent File & Folder Organizer\nScan. Classify. Organize. Undo.\nSHA-256 integrity. Atomic rollback.\n\nMIT License";

// ============================================================
// Command IDs
// ============================================================
enum Cmd {
    CMD_EXIT        = 1,
    CMD_ABOUT       = 2,
    CMD_IMPORT_FILE = 10,
    CMD_IMPORT_DIR  = 11,
    CMD_REFRESH     = 12,
    CMD_RESTORE     = 20,
    CMD_TRASH       = 21,
    CMD_DELETE      = 22,
    CMD_VERIFY      = 23,
    CMD_DETAILS     = 24,
    CMD_ORG_BROWSE  = 100,
    CMD_ORG_SCAN    = 101,
    CMD_ORG_PLAN    = 102,
    CMD_ORG_EXEC    = 103,
    CMD_ORG_UNDO    = 104,
    CMD_ORG_CLEAR   = 105,
};

enum TabPage { PAGE_ARCHIVE = 0, PAGE_ORGANIZER = 1, PAGE_COUNT = 2 };

// ============================================================
// Globals
// ============================================================
static HINSTANCE g_hInst = nullptr;
static HWND g_hWnd = nullptr;
static HWND g_hStatus = nullptr;
static HWND g_hTab = nullptr;

static HWND g_archivePanel = nullptr;
static HWND g_archiveList = nullptr;

static HWND g_orgPanel = nullptr;
static HWND g_orgDropZone = nullptr;
static HWND g_orgStepLabel = nullptr;
static HWND g_orgScanBtn = nullptr;
static HWND g_orgPlanBtn = nullptr;
static HWND g_orgExecBtn = nullptr;
static HWND g_orgUndoBtn = nullptr;
static HWND g_orgClearBtn = nullptr;
static HWND g_orgProgress = nullptr;
static HWND g_orgSummary = nullptr;
static HWND g_orgResultList = nullptr;
static HWND g_orgIntensityLabel = nullptr;
static HWND g_orgIntensitySlider = nullptr;
static int g_orgIntensity = 50;

static HFONT hFontNormal = nullptr;
static HFONT hFontBold = nullptr;
static HFONT hFontBig = nullptr;
static HFONT hFontMono = nullptr;

static app::AppConfig g_config;
static std::vector<core::ArchiveItem> g_items;
static std::wstring g_orgFolder;
static std::string g_lastPlanId;
static std::atomic<bool> g_busy{false};

struct OrgResultRow {
    std::wstring src, dst, conf, reason;
};
static std::vector<OrgResultRow> g_orgResults;

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
    storage::TaxonomyRepository taxonomy;
    filesystem::StorageManager storage;
    services::ProjectDetector detector;
    Svc() : db(g_config.db_path), items(db), categories(db), tags(db),
            activities(db), versions(db), stored(db),
            scans(db), scan_items(db), classifications(db), rules(db),
            plans(db), moves(db), undos(db), taxonomy(db),
            storage(g_config.data_dir, g_config.items_dir) { db.initialize(); }
};

static std::unique_ptr<Svc> g_svc;

// ============================================================
// String helpers
// ============================================================
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

static std::wstring Ellipsis(const std::wstring& p, int max) {
    if ((int)p.size() <= max) return p;
    return L"..." + p.substr(p.size() - (max - 3));
}

// ============================================================
// Font creation
// ============================================================
static HFONT MakeFont(int size, int weight = FW_NORMAL, const wchar_t* name = L"Segoe UI") {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, name);
}

static void InitFonts() {
    hFontNormal = MakeFont(-14);
    hFontBold   = MakeFont(-14, FW_SEMIBOLD);
    hFontBig    = MakeFont(-20, FW_SEMIBOLD);
    hFontMono   = MakeFont(-13, FW_NORMAL, L"Consolas");
}

static void ApplyFont(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); }

// ============================================================
// Theme-aware button
// ============================================================
static HWND MakeBtn(HWND parent, const wchar_t* text, int id, bool primary = false) {
    HWND h = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | (primary ? BS_DEFPUSHBUTTON : 0),
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    ApplyFont(h, hFontNormal);
    return h;
}

// ============================================================
// Status
// ============================================================
static void Status(const wchar_t* txt) {
    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)txt);
}

// ============================================================
// Archive tab
// ============================================================
static void ArchiveRefresh() {
    ListView_DeleteAllItems(g_archiveList);
    try { g_items = g_svc->items.find_all(); }
    catch (...) { Status(L"Error loading items"); return; }
    for (int i = 0; i < (int)g_items.size(); i++) {
        const auto& it = g_items[i];
        std::wstring nm = ToW(it.name);
        LVITEMW li = {};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = (LPWSTR)nm.c_str();
        int idx = ListView_InsertItem(g_archiveList, &li);
        std::wstring tp = ToW(core::to_string(it.type));
        std::wstring st = ToW(core::to_string(it.status));
        std::wstring vr = L"v" + std::to_wstring(it.current_version);
        std::wstring sz = std::to_wstring(it.size) + L" B";
        ListView_SetItemText(g_archiveList, idx, 1, (LPWSTR)tp.c_str());
        ListView_SetItemText(g_archiveList, idx, 2, (LPWSTR)st.c_str());
        ListView_SetItemText(g_archiveList, idx, 3, (LPWSTR)vr.c_str());
        ListView_SetItemText(g_archiveList, idx, 4, (LPWSTR)sz.c_str());
    }
    std::wstring m = std::to_wstring(g_items.size()) + L" item(s)";
    Status(m.c_str());
}

static void ArchiveShowDetails() {
    int i = ListView_GetNextItem(g_archiveList, -1, LVNI_SELECTED);
    if (i < 0 || i >= (int)g_items.size()) return;
    const auto& it = g_items[i];
    std::wstringstream ss;
    ss << L"Name:       " << ToW(it.name) << L"\n"
       << L"ID:         " << ToW(it.id) << L"\n"
       << L"Type:       " << ToW(core::to_string(it.type)) << L"\n"
       << L"Status:     " << ToW(core::to_string(it.status)) << L"\n"
       << L"Size:       " << it.size << L" bytes\n"
       << L"Version:    v" << it.current_version << L"\n"
       << L"Checksum:   " << ToW(it.checksum).substr(0, 32) << L"...\n"
       << L"Original:   " << ToW(it.original_path) << L"\n"
       << L"Created:    " << ToW(it.created_at) << L"\n";
    try {
        auto vers = g_svc->versions.find_by_item(it.id);
        if (!vers.empty()) {
            ss << L"\nVersions (" << vers.size() << L"):\n";
            for (const auto& v : vers)
                ss << L"  v" << v.version_number << L"  " << v.size << L" B  " << ToW(v.created_at) << L"\n";
        }
    } catch (...) {}
    MessageBoxW(g_hWnd, ss.str().c_str(), L"Item Details", MB_ICONINFORMATION);
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
    o.lpstrTitle = L"Select file to import";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    o.lpstrFilter = L"All files (*.*)\0*.*\0";
    return GetOpenFileNameW(&o) ? std::wstring(f) : L"";
}

// ============================================================
// Organizer tab — busy state
// ============================================================
static void OrgSetBusy(bool busy) {
    g_busy = busy;
    EnableWindow(g_orgScanBtn, !busy);
    EnableWindow(g_orgPlanBtn, !busy);
    EnableWindow(g_orgExecBtn, !busy);
    EnableWindow(g_orgUndoBtn, !busy);
    EnableWindow(g_orgClearBtn, !busy);
    if (busy) Status(L"Working...") ; else Status(L"Ready");
}

static void OrgSetStep(const wchar_t* step) {
    SetWindowTextW(g_orgStepLabel, step);
}

static void OrgSetSummary(const wchar_t* txt) {
    SetWindowTextW(g_orgSummary, txt);
}

static void OrgClearResults() {
    g_orgResults.clear();
    ListView_SetItemCountEx(g_orgResultList, 0, 0);
    OrgSetSummary(L"");
}

static void OrgFlushResults() {
    ListView_SetItemCountEx(g_orgResultList, (DWORD)g_orgResults.size(), 0);
    ListView_RedrawItems(g_orgResultList, 0, (int)g_orgResults.size() - 1);
}

// ============================================================
// Organizer — background operations
// ============================================================
static void DoOrgScan(HWND h) {
    if (g_orgFolder.empty()) { MessageBoxW(h, L"Select a folder first.\nDrag a folder onto the Organizer tab or click Browse.", L"Archive", MB_ICONINFORMATION); return; }
    if (g_busy) return;
    OrgSetBusy(true);
    OrgClearResults();
    OrgSetStep(L"Step 1/3 — Scanning files...");
    SendMessageW(g_orgProgress, PBM_SETPOS, 0, 0);

    std::string folder = FromW(g_orgFolder);
    std::thread([h, folder]() {
        try {
            services::Scanner scanner(g_svc->db, g_svc->scans, g_svc->scan_items);
            auto scan = scanner.scan_directory(folder, true);
            PostMessage(h, WM_APP + 100, 0, (LPARAM)new std::string(scan.id));
        } catch (...) { PostMessage(h, WM_APP + 101, 0, 0); }
    }).detach();
}

static void DoOrgPlan(HWND h) {
    if (g_orgFolder.empty()) { MessageBoxW(h, L"Select a folder first.", L"Archive", MB_ICONINFORMATION); return; }
    if (g_busy) return;
    OrgSetBusy(true);
    OrgClearResults();
    g_orgIntensity = (int)SendMessageW(g_orgIntensitySlider, TBM_GETPOS, 0, 0);
    wchar_t intensityMsg[64];
    wsprintfW(intensityMsg, L"Step 2/3 — Analyzing & planning (intensity %d)...", g_orgIntensity);
    OrgSetStep(intensityMsg);
    SendMessageW(g_orgProgress, PBM_SETPOS, 0, 0);

    std::string folder = FromW(g_orgFolder);
    int intensity = g_orgIntensity;
    std::thread([h, folder, intensity]() {
        try {
            services::Scanner scanner(g_svc->db, g_svc->scans, g_svc->scan_items);
            services::Classifier classifier(g_svc->db, g_svc->classifications, g_svc->rules, g_svc->scan_items);
            services::OrganizationPlanner planner(g_svc->db, g_svc->scans, g_svc->scan_items, g_svc->classifications, g_svc->plans, g_svc->moves);
            auto scan = scanner.scan_with_analysis(folder, true);
            PostMessage(h, WM_APP + 110, 0, 0);
            classifier.classify_with_analyses(scan.scan.id, scan.analyses, intensity);
            PostMessage(h, WM_APP + 120, 0, 0);
            auto plan = planner.create_plan_with_analyses(scan.scan.id, folder, scan.analyses, intensity);
            planner.approve_plan(plan.id);
            PostMessage(h, WM_APP + 200, 0, (LPARAM)new std::string(plan.id));
        } catch (...) { PostMessage(h, WM_APP + 201, 0, 0); }
    }).detach();
}

static void DoOrgExec(HWND h) {
    if (g_lastPlanId.empty()) { MessageBoxW(h, L"Run a plan first.", L"Archive", MB_ICONINFORMATION); return; }
    if (g_busy) return;

    auto plan = g_svc->plans.find_by_id(g_lastPlanId);
    if (!plan || plan->status != core::PlanStatus::Ready) {
        MessageBoxW(h, L"No ready plan to execute.", L"Archive", MB_ICONINFORMATION);
        return;
    }

    std::wstring msg = L"Execute plan?\nMoves: " + std::to_wstring(plan->moves_planned)
                     + L"\nRoot:  " + ToW(plan->root_path);
    if (MessageBoxW(h, msg.c_str(), L"Confirm Execution", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    OrgSetBusy(true);
    OrgSetStep(L"Step 3/3 — Executing moves...");
    SendMessageW(g_orgProgress, PBM_SETPOS, 0, 0);

    std::string pid = g_lastPlanId;
    std::thread([h, pid]() {
        try {
            services::OrganizationExecutor executor(g_svc->db, g_svc->plans, g_svc->moves, g_svc->undos);
            auto record = executor.execute(pid);
            PostMessage(h, WM_APP + 300, 0, (LPARAM)new std::string(record.id));
        } catch (...) { PostMessage(h, WM_APP + 301, 0, 0); }
    }).detach();
}

static void DoOrgUndo(HWND h) {
    if (g_busy) return;
    auto all = g_svc->undos.find_all();
    if (all.empty()) { MessageBoxW(h, L"No undo records available.", L"Archive", MB_ICONINFORMATION); return; }

    auto& last = all.back();
    if (last.status == core::UndoStatus::Used) {
        MessageBoxW(h, L"Last undo was already applied.", L"Archive", MB_ICONINFORMATION);
        return;
    }

    std::wstring msg = L"Undo last operation?\nMoves: " + std::to_wstring(last.moves_count)
                     + L"\nRoot:  " + ToW(last.root_path);
    if (MessageBoxW(h, msg.c_str(), L"Confirm Undo", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    OrgSetBusy(true);
    OrgSetStep(L"Undoing moves...");
    SendMessageW(g_orgProgress, PBM_SETPOS, 0, 0);

    std::string uid = last.id;
    std::thread([h, uid]() {
        try {
            services::OrganizationExecutor executor(g_svc->db, g_svc->plans, g_svc->moves, g_svc->undos);
            executor.undo(uid);
            PostMessage(h, WM_APP + 400, 0, 0);
        } catch (...) { PostMessage(h, WM_APP + 401, 0, 0); }
    }).detach();
}

// ============================================================
// Layout
// ============================================================
static void LayoutArchive(RECT rc) {
    MoveWindow(g_archiveList, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

static void LayoutOrganizer(RECT rc) {
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    int x = rc.left, y = rc.top;
    int pad = 10, btnH = 30, lineH = 38, progH = 8;

    int cy = y + pad;

    MoveWindow(g_orgDropZone, x + pad, cy, W - 2 * pad, 44, TRUE);
    cy += 50;

    int bw = 90, gap = 8;
    int totalBtns = 4 * bw + 3 * gap + gap;
    int bx = x + pad + (W - 2 * pad - totalBtns) / 2;

    MoveWindow(g_orgScanBtn, bx, cy, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(g_orgPlanBtn, bx, cy, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(g_orgExecBtn, bx, cy, bw + 20, btnH, TRUE); bx += bw + 20 + gap;
    MoveWindow(g_orgUndoBtn, bx, cy, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(g_orgClearBtn, bx, cy, 60, btnH, TRUE);
    cy += lineH;

    MoveWindow(g_orgProgress, x + pad, cy, W - 2 * pad, progH, TRUE);
    cy += progH + 8;

    MoveWindow(g_orgStepLabel, x + pad, cy, W - 2 * pad, 20, TRUE);
    cy += 22;

    MoveWindow(g_orgSummary, x + pad, cy, W - 2 * pad, 22, TRUE);
    cy += 28;

    MoveWindow(g_orgIntensityLabel, x + pad, cy, 100, 20, TRUE);
    MoveWindow(g_orgIntensitySlider, x + pad + 100, cy, W - 2 * pad - 100, 24, TRUE);
    cy += 28;

    int listH = H - (cy - y) - pad;
    if (listH < 60) listH = 60;
    MoveWindow(g_orgResultList, x + pad, cy, W - 2 * pad, listH, TRUE);
}

static void SwitchTab(int idx) {
    ShowWindow(g_archivePanel, idx == PAGE_ARCHIVE ? SW_SHOW : SW_HIDE);
    ShowWindow(g_orgPanel, idx == PAGE_ORGANIZER ? SW_SHOW : SW_HIDE);
}

// ============================================================
// WndProc
// ============================================================
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX ic = { sizeof(ic),
            ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_PROGRESS_CLASS };
        InitCommonControlsEx(&ic);

        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, h, (HMENU)2000, g_hInst, nullptr);
        ApplyFont(g_hTab, hFontNormal);

        TCITEMW tc = {};
        tc.mask = TCIF_TEXT;
        tc.pszText = (LPWSTR)L"  Archive  ";
        TabCtrl_InsertItem(g_hTab, 0, &tc);
        tc.pszText = (LPWSTR)L"  Organizer  ";
        TabCtrl_InsertItem(g_hTab, 1, &tc);

        // ---- Archive panel ----
        g_archivePanel = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD, 0, 0, 0, 0, h, nullptr, g_hInst, nullptr);

        g_archiveList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, g_archivePanel, (HMENU)1, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_archiveList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

        { struct { const wchar_t* n; int w; } c[] = {
            {L"Name", 240}, {L"Type", 90}, {L"Status", 90}, {L"Version", 80}, {L"Size", 110}
        }; LVCOLUMNW col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        for (int i = 0; i < 5; i++) { col.pszText = (LPWSTR)c[i].n; col.cx = c[i].w; ListView_InsertColumn(g_archiveList, i, &col); }}

        // ---- Organizer panel ----
        g_orgPanel = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD, 0, 0, 0, 0, h, nullptr, g_hInst, nullptr);

        g_orgDropZone = CreateWindowExW(WS_EX_ACCEPTFILES, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, g_orgPanel, nullptr, g_hInst, nullptr);
        ApplyFont(g_orgDropZone, hFontNormal);

        g_orgScanBtn  = MakeBtn(g_orgPanel, L"Scan", CMD_ORG_SCAN, false);
        g_orgPlanBtn  = MakeBtn(g_orgPanel, L"Plan", CMD_ORG_PLAN, false);
        g_orgExecBtn  = MakeBtn(g_orgPanel, L"Execute", CMD_ORG_EXEC, true);
        g_orgUndoBtn  = MakeBtn(g_orgPanel, L"Undo", CMD_ORG_UNDO, false);
        g_orgClearBtn = MakeBtn(g_orgPanel, L"Clear", CMD_ORG_CLEAR, false);

        g_orgProgress = CreateWindowExW(0, PROGRESS_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            0, 0, 0, 0, g_orgPanel, nullptr, g_hInst, nullptr);
        SendMessageW(g_orgProgress, PBM_SETRANGE32, 0, 1000);
        SendMessageW(g_orgProgress, PBM_SETBARCOLOR, 0, (LPARAM)CLR_ACCENT);
        SendMessageW(g_orgProgress, PBM_SETBKCOLOR, 0, (LPARAM)CLR_BG_INPUT);

        g_orgStepLabel = CreateWindowExW(0, L"STATIC", L"Select a folder to begin",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, g_orgPanel, nullptr, g_hInst, nullptr);
        ApplyFont(g_orgStepLabel, hFontNormal);

        g_orgSummary = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, g_orgPanel, nullptr, g_hInst, nullptr);
        ApplyFont(g_orgSummary, hFontMono);

        g_orgIntensityLabel = CreateWindowExW(0, L"STATIC", L"Intensity: 50",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, g_orgPanel, nullptr, g_hInst, nullptr);
        ApplyFont(g_orgIntensityLabel, hFontNormal);

        g_orgIntensitySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
            0, 0, 0, 0, g_orgPanel, (HMENU)2001, g_hInst, nullptr);
        SendMessageW(g_orgIntensitySlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(g_orgIntensitySlider, TBM_SETPOS, TRUE, 50);
        SendMessageW(g_orgIntensitySlider, TBM_SETTICFREQ, 10, 0);

        g_orgResultList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
            0, 0, 0, 0, g_orgPanel, (HMENU)2, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_orgResultList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

        { struct { const wchar_t* n; int w; } c[] = {
            {L"Source", 260}, {L"Destination", 260}, {L"Confidence", 90}, {L"Reason", 220}
        }; LVCOLUMNW col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        for (int i = 0; i < 4; i++) { col.pszText = (LPWSTR)c[i].n; col.cx = c[i].w; ListView_InsertColumn(g_orgResultList, i, &col); }}

        DragAcceptFiles(g_orgPanel, TRUE);

        // Status bar
        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, h, (HMENU)3000, g_hInst, nullptr);
        ApplyFont(g_hStatus, hFontNormal);
        int parts[] = { 300, 600, -1 };
        SendMessageW(g_hStatus, SB_SETPARTS, 3, (LPARAM)parts);

        SwitchTab(PAGE_ARCHIVE);

        // Explicit dark ListView colors (WM_CTLCOLORLISTBOX doesn't apply to LVS_REPORT)
        ListView_SetBkColor(g_archiveList, CLR_BG);
        ListView_SetTextBkColor(g_archiveList, CLR_BG);
        ListView_SetTextColor(g_archiveList, CLR_TEXT);
        ListView_SetBkColor(g_orgResultList, CLR_BG);
        ListView_SetTextBkColor(g_orgResultList, CLR_BG);
        ListView_SetTextColor(g_orgResultList, CLR_TEXT);

        // Dark tab background
        SendMessageW(g_hTab, WM_CTLCOLORSTATIC, (WPARAM)GetDC(g_hTab), (LPARAM)g_hTab);

        // Defer init — don't block the message loop
        PostMessage(h, WM_APP + 500, 0, 0);
        break;
    }
    case WM_SIZE: {
        int ww = LOWORD(l), hh = HIWORD(l);
        SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT sr; GetWindowRect(g_hStatus, &sr);
        int statusH = sr.bottom - sr.top;
        RECT trc = { 0, 0, ww, hh - statusH };
        TabCtrl_AdjustRect(g_hTab, FALSE, &trc);
        MoveWindow(g_hTab, trc.left, trc.top, trc.right - trc.left, trc.bottom - trc.top, TRUE);
        RECT arc; TabCtrl_AdjustRect(g_hTab, TRUE, &arc);
        LayoutArchive(arc);
        LayoutOrganizer(arc);
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        HWND hw = (HWND)l;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG);
        if (hw == g_orgDropZone) { SetBkColor(hdc, CLR_BG_LIGHT); return (LRESULT)hbrBgLight; }
        return (LRESULT)hbrBg;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_SURFACE);
        return (LRESULT)hbrSurface;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG_INPUT);
        return (LRESULT)hbrBgInput;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG_LIGHT);
        return (LRESULT)hbrBgLight;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)w;
        wchar_t path[MAX_PATH] = L"";
        DragQueryFileW(hd, 0, path, MAX_PATH);
        DragFinish(hd);
        g_orgFolder = path;
        InvalidateRect(g_orgDropZone, nullptr, TRUE);
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)l;
        if (ds->CtlID == 0 && ds->hwndItem == g_orgDropZone) {
            HDC hdc = ds->hDC;
            RECT rc = ds->rcItem;
            HBRUSH br = CreateSolidBrush(CLR_BG_LIGHT);
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            HPEN hPen = CreatePen(PS_DOT, 2, CLR_ACCENT);
            HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 8, 8);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(hPen);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_orgFolder.empty() ? CLR_TEXT_DIM : CLR_TEXT);
            HFONT oldF = (HFONT)SelectObject(hdc, hFontBold);
            const wchar_t* txt = g_orgFolder.empty() ? L"Drop a folder here, or click Browse" : Ellipsis(g_orgFolder, 90).c_str();
            DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldF);
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)l;
        if (nm->hwndFrom == g_hTab && nm->code == TCN_SELCHANGE) {
            SwitchTab(TabCtrl_GetCurSel(g_hTab));
        }
        if (nm->hwndFrom == g_archiveList) {
            if (nm->code == NM_DBLCLK) ArchiveShowDetails();
            if (nm->code == NM_RCLICK) {
                int i = ListView_GetNextItem(g_archiveList, -1, LVNI_SELECTED);
                if (i < 0 || i >= (int)g_items.size()) break;
                HMENU hPop = CreatePopupMenu();
                AppendMenuW(hPop, MF_STRING, CMD_DETAILS, L"&Details");
                AppendMenuW(hPop, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hPop, MF_STRING, CMD_RESTORE, L"&Restore");
                AppendMenuW(hPop, MF_STRING, CMD_TRASH, L"&Trash");
                AppendMenuW(hPop, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hPop, MF_STRING, CMD_DELETE, L"&Delete permanently");
                POINT pt; GetCursorPos(&pt);
                TrackPopupMenu(hPop, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
                DestroyMenu(hPop);
            }
        }
        if (nm->hwndFrom == g_orgResultList && nm->code == LVN_GETDISPINFO) {
            NMLVDISPINFO* di = (NMLVDISPINFO*)l;
            int row = di->item.iItem;
            if (row < 0 || row >= (int)g_orgResults.size()) break;
            const auto& r = g_orgResults[row];
            if (di->item.mask & LVIF_TEXT) {
                const wchar_t* txt = L"";
                switch (di->item.iSubItem) {
                    case 0: txt = r.src.c_str(); break;
                    case 1: txt = r.dst.c_str(); break;
                    case 2: txt = r.conf.c_str(); break;
                    case 3: txt = r.reason.c_str(); break;
                }
                int len = lstrlenW(txt);
                if (len >= di->item.cchTextMax) len = di->item.cchTextMax - 1;
                wmemcpy(di->item.pszText, txt, len);
                di->item.pszText[len] = L'\0';
            }
        }
        break;
    }
    case WM_HSCROLL: {
        if ((HWND)l == g_orgIntensitySlider) {
            int pos = (int)SendMessageW(g_orgIntensitySlider, TBM_GETPOS, 0, 0);
            wchar_t buf[32];
            wsprintfW(buf, L"Intensity: %d", pos);
            SetWindowTextW(g_orgIntensityLabel, buf);
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case CMD_EXIT: DestroyWindow(h); break;
        case CMD_ABOUT: MessageBoxW(h, APP_ABOUT, L"About Archive", MB_ICONINFORMATION); break;
        case CMD_IMPORT_FILE: {
            std::wstring path = PickFile(h);
            if (path.empty()) return 0;
            Status(L"Importing...");
            UpdateWindow(h);
            try {
                services::ImportService imp(g_svc->db, g_svc->items, g_svc->categories, g_svc->tags,
                    g_svc->activities, g_svc->versions, g_svc->stored, g_svc->storage, g_svc->detector);
                core::ImportRequest req; req.paths.push_back(FromW(path));
                auto res = imp.import(req);
                if (res.has_errors())
                    MessageBoxW(h, (L"Error: " + ToW(res.errors[0].error)).c_str(), L"Import Error", MB_ICONERROR);
                ArchiveRefresh();
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_IMPORT_DIR: {
            std::wstring path = PickFolder(h);
            if (path.empty()) return 0;
            Status(L"Importing folder...");
            UpdateWindow(h);
            try {
                services::ImportService imp(g_svc->db, g_svc->items, g_svc->categories, g_svc->tags,
                    g_svc->activities, g_svc->versions, g_svc->stored, g_svc->storage, g_svc->detector);
                core::ImportRequest req; req.paths.push_back(FromW(path));
                auto res = imp.import(req);
                if (res.has_errors())
                    MessageBoxW(h, (L"Error: " + ToW(res.errors[0].error)).c_str(), L"Import Error", MB_ICONERROR);
                ArchiveRefresh();
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_REFRESH: ArchiveRefresh(); break;
        case CMD_DETAILS: ArchiveShowDetails(); break;
        case CMD_RESTORE: {
            int i = ListView_GetNextItem(g_archiveList, -1, LVNI_SELECTED);
            if (i < 0 || i >= (int)g_items.size()) return 0;
            const auto& it = g_items[i];
            if (MessageBoxW(h, (L"Restore \"" + ToW(it.name) + L"\"?").c_str(), L"Restore",
                MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            try {
                services::VersionService vs(g_svc->db, g_svc->versions, g_svc->items, g_svc->activities, g_svc->stored, g_svc->storage);
                auto lv = g_svc->versions.find_latest(it.id);
                if (!lv) { MessageBoxW(h, L"No versions found.", L"Error", MB_ICONERROR); return 0; }
                vs.restore(it.id, lv->id);
                MessageBoxW(h, (L"Restored v" + std::to_wstring(lv->version_number)).c_str(), L"Done", MB_ICONINFORMATION);
                ArchiveRefresh();
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_TRASH: {
            int i = ListView_GetNextItem(g_archiveList, -1, LVNI_SELECTED);
            if (i < 0 || i >= (int)g_items.size()) return 0;
            const auto& it = g_items[i];
            if (MessageBoxW(h, (L"Trash \"" + ToW(it.name) + L"\"?").c_str(), L"Trash",
                MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            try {
                services::UpdateService u(g_svc->db, g_svc->items, g_svc->activities, g_svc->storage);
                u.move_to_trash(it.id);
                ArchiveRefresh();
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_DELETE: {
            int i = ListView_GetNextItem(g_archiveList, -1, LVNI_SELECTED);
            if (i < 0 || i >= (int)g_items.size()) return 0;
            const auto& it = g_items[i];
            if (MessageBoxW(h, (L"PERMANENTLY delete \"" + ToW(it.name) + L"\"?\nThis cannot be undone.").c_str(),
                L"Delete", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
            try {
                services::UpdateService u(g_svc->db, g_svc->items, g_svc->activities, g_svc->storage);
                u.permanent_delete(it.id);
                ArchiveRefresh();
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_VERIFY: {
            Status(L"Verifying integrity...");
            UpdateWindow(h);
            try {
                services::IntegrityService integ(g_svc->items, g_svc->versions, g_svc->stored, g_svc->activities, g_svc->storage);
                auto r = integ.verify_all();
                std::wstringstream ss;
                ss << L"Valid: " << r.valid_count;
                if (r.modified_count) ss << L"  |  Modified: " << r.modified_count;
                if (r.missing_count) ss << L"  |  Missing: " << r.missing_count;
                MessageBoxW(h, ss.str().c_str(), L"Verification",
                    r.all_valid() ? MB_ICONINFORMATION : MB_ICONWARNING);
                Status(ss.str().c_str());
            } catch (const std::exception& e) { MessageBoxW(h, ToW(e.what()).c_str(), L"Error", MB_ICONERROR); }
            break;
        }
        case CMD_ORG_BROWSE: {
            std::wstring f = PickFolder(h);
            if (!f.empty()) {
                g_orgFolder = f;
                InvalidateRect(g_orgDropZone, nullptr, TRUE);
            }
            break;
        }
        case CMD_ORG_SCAN:   DoOrgScan(h); break;
        case CMD_ORG_PLAN:   DoOrgPlan(h); break;
        case CMD_ORG_EXEC:   DoOrgExec(h); break;
        case CMD_ORG_UNDO:   DoOrgUndo(h); break;
        case CMD_ORG_CLEAR:  OrgClearResults(); OrgSetStep(L"Select a folder to begin"); break;
        }
        break;

    // ---- Background thread results ----
    case WM_APP + 100: {
        std::string* id = (std::string*)l;
        OrgSetBusy(false);
        OrgSetStep(L"Scan complete");
        SendMessageW(g_orgProgress, PBM_SETPOS, 1000, 0);
        OrgSetSummary((L"Scan complete  |  Scan ID: " + ToW(*id)).c_str());
        delete id;
        break;
    }
    case WM_APP + 101:
        OrgSetBusy(false);
        OrgSetStep(L"Scan failed");
        MessageBoxW(h, L"Scan failed. Check the folder path and try again.", L"Error", MB_ICONERROR);
        break;
    case WM_APP + 110:
        SendMessageW(g_orgProgress, PBM_SETPOS, 200, 0);
        break;
    case WM_APP + 120:
        SendMessageW(g_orgProgress, PBM_SETPOS, 500, 0);
        OrgSetStep(L"Step 2/3 — Classifying files...");
        break;
    case WM_APP + 200: {
        std::string* pid = (std::string*)l;
        g_lastPlanId = *pid;
        OrgSetBusy(false);
        OrgSetStep(L"Plan ready — review moves below");
        SendMessageW(g_orgProgress, PBM_SETPOS, 1000, 0);
        g_orgResults.clear();
        auto summary = g_svc->moves.find_by_plan(*pid);
        g_orgResults.reserve(summary.size());
        for (const auto& m : summary) {
            std::wstringstream conf;
            conf << std::fixed << std::setprecision(0) << (m.confidence * 100.0) << L"%";
            g_orgResults.push_back({
                Ellipsis(ToW(m.source_path), 80),
                Ellipsis(ToW(m.dest_path), 80),
                conf.str(),
                ToW(m.reason)
            });
        }
        OrgFlushResults();
        std::wstringstream ss;
        ss << summary.size() << L" moves planned  |  Plan ID: " << ToW(*pid);
        OrgSetSummary(ss.str().c_str());
        delete pid;
        break;
    }
    case WM_APP + 201:
        OrgSetBusy(false);
        OrgSetStep(L"Plan failed");
        MessageBoxW(h, L"Planning failed. Check folder contents.", L"Error", MB_ICONERROR);
        break;
    case WM_APP + 300: {
        std::string* uid = (std::string*)l;
        OrgSetBusy(false);
        OrgSetStep(L"Execution complete");
        SendMessageW(g_orgProgress, PBM_SETPOS, 1000, 0);
        OrgSetSummary((L"Executed!  |  Undo ID: " + ToW(*uid) + L"  — click Undo to revert").c_str());
        delete uid;
        break;
    }
    case WM_APP + 301:
        OrgSetBusy(false);
        OrgSetStep(L"Execution failed");
        MessageBoxW(h, L"Execution failed.", L"Error", MB_ICONERROR);
        break;
    case WM_APP + 400:
        OrgSetBusy(false);
        OrgSetStep(L"Undo complete — files restored");
        SendMessageW(g_orgProgress, PBM_SETPOS, 1000, 0);
        OrgSetSummary(L"All files restored to their original locations.");
        OrgClearResults();
        break;
    case WM_APP + 401:
        OrgSetBusy(false);
        OrgSetStep(L"Undo failed");
        MessageBoxW(h, L"Undo failed.", L"Error", MB_ICONERROR);
        break;

    case WM_APP + 500:
        ArchiveRefresh();
        break;

    case WM_DESTROY:
        if (hFontNormal) DeleteObject(hFontNormal);
        if (hFontBold) DeleteObject(hFontBold);
        if (hFontBig) DeleteObject(hFontBig);
        if (hFontMono) DeleteObject(hFontMono);
        if (hbrBg) DeleteObject(hbrBg);
        if (hbrBgLight) DeleteObject(hbrBgLight);
        if (hbrBgInput) DeleteObject(hbrBgInput);
        if (hbrSurface) DeleteObject(hbrSurface);
        if (hbrBorder) DeleteObject(hbrBorder);
        g_svc.reset();
        PostQuitMessage(0);
        break;
    default: return DefWindowProcW(h, m, w, l);
    }
    return 0;
}

// ============================================================
// WinMain
// ============================================================
int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nS) {
    g_hInst = hI;
    g_config = app::AppConfig::default_config();
    g_config.ensure_directories();
    g_svc = std::make_unique<Svc>();

    InitBrushes();
    InitFonts();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = hbrBg;
    wc.lpszClassName = L"ArchiveWnd";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HMENU hBar = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, CMD_IMPORT_FILE, L"Import &File...\tCtrl+I");
    AppendMenuW(hFile, MF_STRING, CMD_IMPORT_DIR,  L"Import &Folder...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, CMD_EXIT,        L"E&xit\tAlt+F4");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hAct = CreatePopupMenu();
    AppendMenuW(hAct, MF_STRING, CMD_REFRESH, L"&Refresh\tF5");
    AppendMenuW(hAct, MF_STRING, CMD_DETAILS, L"&Details\tEnter");
    AppendMenuW(hAct, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAct, MF_STRING, CMD_RESTORE, L"&Restore");
    AppendMenuW(hAct, MF_STRING, CMD_TRASH,   L"&Trash\tDel");
    AppendMenuW(hAct, MF_STRING, CMD_DELETE,  L"Delete &Permanently");
    AppendMenuW(hAct, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAct, MF_STRING, CMD_VERIFY,  L"&Verify Integrity");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hAct, L"&Actions");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, CMD_ABOUT, L"&About Archive");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    g_hWnd = CreateWindowExW(0, L"ArchiveWnd", APP_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700,
        nullptr, hBar, hI, nullptr);

    // Set min size
    MINMAXINFO mmi = {};
    mmi.ptMinTrackSize.x = 800;
    mmi.ptMinTrackSize.y = 500;
    SendMessageW(g_hWnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);

    // Center on screen
    RECT wr; GetWindowRect(g_hWnd, &wr);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_hWnd, nullptr, (sw - (wr.right - wr.left)) / 2,
        (sh - (wr.bottom - wr.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(g_hWnd, nS);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
