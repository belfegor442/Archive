#include "DashboardWidget.h"

#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>

DashboardWidget::DashboardWidget(archive::services::DashboardService& svc, QWidget* parent)
    : QWidget(parent)
    , svc_(svc)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* layout = new QGridLayout(container);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto make_card = [](const QString& title, QLabel*& value_label, const QString& color) -> QFrame* {
        auto* card = new QFrame();
        card->setStyleSheet(QString(R"(
            QFrame {
                background-color: #1e1e2e;
                border-radius: 12px;
                padding: 20px;
                border-left: 4px solid %1;
            }
        )").arg(color));

        auto* card_layout = new QVBoxLayout(card);
        auto* title_lbl = new QLabel(title);
        title_lbl->setStyleSheet("color: #a6adc8; font-size: 13px;");
        card_layout->addWidget(title_lbl);

        value_label = new QLabel("0");
        value_label->setStyleSheet("color: #cdd6f4; font-size: 28px; font-weight: bold;");
        card_layout->addWidget(value_label);

        return card;
    };

    layout->addWidget(make_card(tr("Total Items"), total_label_, "#89b4fa"), 0, 0);
    layout->addWidget(make_card(tr("Archived"), archived_label_, "#a6e3a1"), 0, 1);
    layout->addWidget(make_card(tr("Favorites"), favorites_label_, "#f38ba8"), 0, 2);
    layout->addWidget(make_card(tr("Trash"), deleted_label_, "#f9e2af"), 1, 0);
    layout->addWidget(make_card(tr("Total Size"), size_label_, "#cba6f7"), 1, 1);

    layout->setRowStretch(2, 1);
    layout->setColumnStretch(3, 1);

    scroll->setWidget(container);
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(scroll);

    setStyleSheet("background-color: #11111b;");
    refresh();
}

void DashboardWidget::refresh() {
    auto stats = svc_.get_stats();
    total_label_->setText(QString::number(stats.total_items));
    archived_label_->setText(QString::number(stats.archived_items));
    favorites_label_->setText(QString::number(stats.favorite_items));
    deleted_label_->setText(QString::number(stats.deleted_items));
    size_label_->setText(format_size(stats.total_size));
}

QString DashboardWidget::format_size(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f %s", size, units[unit]);
    return QString::fromLatin1(buf);
}
