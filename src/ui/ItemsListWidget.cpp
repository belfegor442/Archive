#include "ItemsListWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

ItemsListWidget::ItemsListWidget(archive::services::SearchService& svc, QWidget* parent)
    : QWidget(parent)
    , svc_(svc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(tr("Search items..."));
    search_box_->setStyleSheet(R"(
        QLineEdit {
            background-color: #1e1e2e;
            border: 1px solid #313244;
            border-radius: 8px;
            padding: 10px 14px;
            color: #cdd6f4;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #6366f1;
        }
    )");
    layout->addWidget(search_box_);

    list_ = new QListWidget(this);
    list_->setStyleSheet(R"(
        QListWidget {
            background-color: transparent;
            border: none;
        }
        QListWidget::item {
            background-color: #1e1e2e;
            border-radius: 8px;
            padding: 12px 16px;
            margin-bottom: 4px;
            color: #cdd6f4;
        }
        QListWidget::item:selected {
            background-color: #313244;
        }
        QListWidget::item:hover {
            background-color: #313244;
        }
    )");
    layout->addWidget(list_);

    setStyleSheet("background-color: #11111b;");

    connect(search_box_, &QLineEdit::textChanged, this, &ItemsListWidget::on_search_changed);
    connect(list_, &QListWidget::currentRowChanged, this, &ItemsListWidget::on_row_changed);

    refresh();
}

void ItemsListWidget::refresh() {
    list_->clear();

    std::vector<archive::core::ArchiveItem> items;

    if (mode_ == Mode::Favorites) {
        items = svc_.find_favorites();
    } else if (mode_ == Mode::Trash) {
        items = svc_.find_by_status(archive::core::ItemStatus::Deleted);
    } else {
        auto result = svc_.search(search_box_->text().toStdString());
        items = std::move(result.items);
    }

    for (const auto& item : items) {
        QString display = QString::fromStdString(item.name);
        if (item.is_favorite) display += "  *";
        display += "  [" + QString::fromStdString(archive::core::to_string(item.type)) + "]";

        auto* list_item = new QListWidgetItem(display);
        list_item->setData(Qt::UserRole, QString::fromStdString(item.id));
        list_->addItem(list_item);
    }
}

void ItemsListWidget::set_mode(Mode mode) {
    mode_ = mode;
}

void ItemsListWidget::on_search_changed(const QString& text) {
    (void)text;
    refresh();
}

void ItemsListWidget::on_row_changed(int row) {
    if (row < 0) return;
    QListWidgetItem* item = list_->item(row);
    if (item) {
        QString id = item->data(Qt::UserRole).toString();
        emit item_selected(id);
    }
}
