#include "SidebarWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* title = new QLabel(tr("Archive"));
    title->setAlignment(Qt::AlignCenter);
    QFont title_font = title->font();
    title_font.setPointSize(16);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setStyleSheet("padding: 16px; color: #6366f1;");
    layout->addWidget(title);

    list_ = new QListWidget(this);
    list_->setStyleSheet(R"(
        QListWidget {
            background-color: #1e1e2e;
            border: none;
            padding: 8px;
        }
        QListWidget::item {
            padding: 10px 12px;
            border-radius: 6px;
            color: #cdd6f4;
            margin-bottom: 2px;
        }
        QListWidget::item:selected {
            background-color: #313244;
            color: #cdd6f4;
        }
        QListWidget::item:hover {
            background-color: #313244;
        }
    )");

    list_->addItem(tr("Dashboard"));
    list_->addItem(tr("All Items"));
    list_->addItem(tr("Favorites"));
    list_->addItem(tr("Trash"));

    connect(list_, &QListWidget::currentRowChanged, this, &SidebarWidget::on_row_changed);

    layout->addWidget(list_);
    layout->addStretch();

    list_->setCurrentRow(0);
}

void SidebarWidget::on_row_changed(int row) {
    if (row < 0) return;
    QListWidgetItem* item = list_->item(row);
    if (!item) return;
    QString text = item->text();
    if (text == tr("Dashboard"))
        emit item_clicked("dashboard");
    else if (text == tr("All Items"))
        emit item_clicked("all");
    else if (text == tr("Favorites"))
        emit item_clicked("favorites");
    else if (text == tr("Trash"))
        emit item_clicked("trash");
}
