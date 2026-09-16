#include "MainWindow.h"
#include "ImportDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
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
    stored_objects_ = std::make_unique<archive::storage::StoredObjectRepository>(*db_);
    storage_ = std::make_unique<archive::filesystem::StorageManager>(config_.data_dir, config_.items_dir);
    detector_ = std::make_unique<archive::services::ProjectDetector>();
    import_svc_ = std::make_unique<archive::services::ImportService>(*db_, *items_, *categories_, *tags_, *activities_, *versions_, *stored_objects_, *storage_, *detector_);
    search_svc_ = std::make_unique<archive::services::SearchService>(*items_);
    dashboard_svc_ = std::make_unique<archive::services::DashboardService>(*items_);
    note_svc_ = std::make_unique<archive::services::NoteService>(*notes_, *items_, *activities_);
    version_svc_ = std::make_unique<archive::services::VersionService>(*db_, *versions_, *items_, *activities_, *stored_objects_, *storage_);
    activity_svc_ = std::make_unique<archive::services::ActivityService>(*activities_);
    integrity_svc_ = std::make_unique<archive::services::IntegrityService>(*items_, *versions_, *stored_objects_, *activities_, *storage_);
    category_svc_ = std::make_unique<archive::services::CategoryService>(*categories_, *items_, *activities_);
    update_svc_ = std::make_unique<archive::services::UpdateService>(*db_, *items_, *activities_, *storage_);

    setup_ui();
    setup_connections();

    content_->setCurrentWidget(dashboard_);
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

    item_detail_ = new ItemDetailWidget(*items_, *note_svc_, *version_svc_, *activity_svc_, *update_svc_, this);
    content_->addWidget(item_detail_);

    setCentralWidget(central);

    auto* file_menu = menuBar()->addMenu(tr("&File"));
    auto* import_action = file_menu->addAction(tr("&Import..."));
    import_action->setShortcut(QKeySequence::New);
    connect(import_action, &QAction::triggered, this, &MainWindow::on_import_clicked);

    file_menu->addSeparator();
    auto* exit_action = file_menu->addAction(tr("E&xit"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto* tools_menu = menuBar()->addMenu(tr("&Tools"));
    auto* verify_action = tools_menu->addAction(tr("&Verify Integrity"));
    connect(verify_action, &QAction::triggered, this, &MainWindow::on_verify_integrity);
}

void MainWindow::setup_connections() {
    connect(sidebar_, &SidebarWidget::item_clicked, this, &MainWindow::navigate_to);
    connect(items_list_, &ItemsListWidget::item_selected, this, &MainWindow::on_item_selected);
}

void MainWindow::navigate_to(const QString& page) {
    if (page == "dashboard") {
        content_->setCurrentWidget(dashboard_);
        dashboard_->refresh();
    } else if (page == "all") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_mode(ItemsListWidget::Mode::All);
        items_list_->refresh();
    } else if (page == "favorites") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_mode(ItemsListWidget::Mode::Favorites);
        items_list_->refresh();
    } else if (page == "trash") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_mode(ItemsListWidget::Mode::Trash);
        items_list_->refresh();
    } else if (page == "categories") {
        content_->setCurrentWidget(items_list_);
        items_list_->set_mode(ItemsListWidget::Mode::All);
        items_list_->refresh();
    }
}

void MainWindow::on_item_selected(const QString& item_id) {
    item_detail_->load_item(item_id);
    content_->setCurrentWidget(item_detail_);
}

void MainWindow::on_import_clicked() {
    ImportDialog dlg(import_svc_.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        dashboard_->refresh();
        items_list_->refresh();
    }
}

void MainWindow::on_verify_integrity() {
    auto result = integrity_svc_->verify_all();

    QString msg;
    if (result.all_valid()) {
        msg = tr("All items verified successfully!\n\n"
                 "Valid: %1\nTotal: %2")
              .arg(result.valid_count)
              .arg(result.items.size());
    } else {
        msg = tr("Integrity issues found:\n\n"
                 "Valid: %1\nModified: %2\nMissing: %3\nCorrupted: %4")
              .arg(result.valid_count)
              .arg(result.modified_count)
              .arg(result.missing_count)
              .arg(result.corrupted_count);
    }

    QMessageBox::information(this, tr("Integrity Check"), msg);
}
