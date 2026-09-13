#include "MainWindow.h"
#include "ImportDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(const archive::app::AppConfig& config, QWidget* parent)
    : QMainWindow(parent)
    , config_(config)
{
    config_.ensure_directories();

    db_ = std::make_unique<archive::storage::DatabaseManager>(config_.db_path);
    db_->initialize();

    items_ = std::make_unique<archive::storage::ArchiveItemRepository>(*db_);
    categories_ = std::make_unique<archive::storage::CategoryRepository>(*db_);
    tags_ = std::make_unique<archive::storage::TagRepository>(*db_);
    versions_ = std::make_unique<archive::storage::VersionRepository>(*db_);
    notes_ = std::make_unique<archive::storage::NoteRepository>(*db_);
    activities_ = std::make_unique<archive::storage::ActivityRepository>(*db_);
    storage_ = std::make_unique<archive::filesystem::StorageManager>(config_.data_dir, config_.items_dir);
    detector_ = std::make_unique<archive::services::ProjectDetector>();
    import_svc_ = std::make_unique<archive::services::ImportService>(*items_, *categories_, *tags_, *activities_, *storage_, *detector_);
    search_svc_ = std::make_unique<archive::services::SearchService>(*items_);
    dashboard_svc_ = std::make_unique<archive::services::DashboardService>(*items_);
    note_svc_ = std::make_unique<archive::services::NoteService>(*notes_, *items_, *activities_);
    version_svc_ = std::make_unique<archive::services::VersionService>(*versions_, *items_, *activities_, *storage_);
    activity_svc_ = std::make_unique<archive::services::ActivityService>(*activities_);

    setup_ui();
    setup_connections();
    refresh_dashboard();

    setWindowTitle("Archive");
    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new SidebarWidget(this);
    sidebar_->setFixedWidth(220);
    layout->addWidget(sidebar_);

    content_ = new QStackedWidget(this);
    layout->addWidget(content_);

    dashboard_ = new DashboardWidget(*dashboard_svc_, this);
    content_->addWidget(dashboard_);

    items_list_ = new ItemsListWidget(*search_svc_, this);
    content_->addWidget(items_list_);

    item_detail_ = new ItemDetailWidget(*items_, *note_svc_, *version_svc_, *activity_svc_, this);
    content_->addWidget(item_detail_);

    setCentralWidget(central);

    auto* import_action = new QAction(tr("&Import"), this);
    import_action->setShortcut(QKeySequence::New);
    menuBar()->addAction(import_action);
    connect(import_action, &QAction::triggered, this, &MainWindow::on_import_clicked);
}

void MainWindow::setup_connections() {
    connect(sidebar_, &SidebarWidget::item_clicked, this, &MainWindow::on_sidebar_item_clicked);
    connect(items_list_, &ItemsListWidget::item_selected, this, &MainWindow::on_item_selected);
}

void MainWindow::on_sidebar_item_clicked(const QString& page) {
    if (page == "dashboard") {
        content_->setCurrentWidget(dashboard_);
        refresh_dashboard();
    } else if (page == "all") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_filters("", false);
        refresh_items_list();
    } else if (page == "favorites") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_filters("", true);
        refresh_items_list();
    } else if (page == "trash") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_status_filter(archive::core::ItemStatus::Deleted);
        refresh_items_list();
    }
}

void MainWindow::on_item_selected(const QString& item_id) {
    item_detail_->load_item(item_id);
    content_->setCurrentWidget(item_detail_);
}

void MainWindow::on_import_clicked() {
    ImportDialog dlg(import_svc_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh_items_list();
        refresh_dashboard();
    }
}

void MainWindow::refresh_dashboard() {
    dashboard_->refresh();
}

void MainWindow::refresh_items_list() {
    items_list_->refresh();
}
