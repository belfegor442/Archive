#pragma once

#include <QMainWindow>
#include <QStackedWidget>

#include "../storage/DatabaseManager.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/CategoryRepository.h"
#include "../storage/TagRepository.h"
#include "../storage/VersionRepository.h"
#include "../storage/NoteRepository.h"
#include "../storage/ActivityRepository.h"
#include "../storage/StoredObjectRepository.h"
#include "../filesystem/StorageManager.h"
#include "../services/ImportService.h"
#include "../services/SearchService.h"
#include "../services/DashboardService.h"
#include "../services/NoteService.h"
#include "../services/VersionService.h"
#include "../services/ActivityService.h"
#include "../services/ProjectDetector.h"
#include "../services/IntegrityService.h"
#include "../services/CategoryService.h"
#include "../services/UpdateService.h"
#include "../app/AppConfig.h"

#include "SidebarWidget.h"
#include "DashboardWidget.h"
#include "ItemsListWidget.h"
#include "ItemDetailWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const archive::app::AppConfig& config, QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void navigate_to(const QString& page);
    void on_item_selected(const QString& item_id);

private slots:
    void on_import_clicked();
    void on_verify_integrity();

private:
    archive::app::AppConfig config_;

    std::unique_ptr<archive::storage::DatabaseManager> db_;
    std::unique_ptr<archive::storage::ArchiveItemRepository> items_;
    std::unique_ptr<archive::storage::CategoryRepository> categories_;
    std::unique_ptr<archive::storage::TagRepository> tags_;
    std::unique_ptr<archive::storage::VersionRepository> versions_;
    std::unique_ptr<archive::storage::NoteRepository> notes_;
    std::unique_ptr<archive::storage::ActivityRepository> activities_;
    std::unique_ptr<archive::storage::StoredObjectRepository> stored_objects_;
    std::unique_ptr<archive::filesystem::StorageManager> storage_;
    std::unique_ptr<archive::services::ProjectDetector> detector_;
    std::unique_ptr<archive::services::ImportService> import_svc_;
    std::unique_ptr<archive::services::SearchService> search_svc_;
    std::unique_ptr<archive::services::DashboardService> dashboard_svc_;
    std::unique_ptr<archive::services::NoteService> note_svc_;
    std::unique_ptr<archive::services::VersionService> version_svc_;
    std::unique_ptr<archive::services::ActivityService> activity_svc_;
    std::unique_ptr<archive::services::IntegrityService> integrity_svc_;
    std::unique_ptr<archive::services::CategoryService> category_svc_;
    std::unique_ptr<archive::services::UpdateService> update_svc_;

    SidebarWidget* sidebar_ = nullptr;
    QStackedWidget* content_ = nullptr;
    DashboardWidget* dashboard_ = nullptr;
    ItemsListWidget* items_list_ = nullptr;
    ItemDetailWidget* item_detail_ = nullptr;

    void setup_ui();
    void setup_connections();
};
