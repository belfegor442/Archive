#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

#include "../services/DashboardService.h"

class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(archive::services::DashboardService& svc, QWidget* parent = nullptr);
    void refresh();

private:
    archive::services::DashboardService& svc_;

    QLabel* total_label_ = nullptr;
    QLabel* archived_label_ = nullptr;
    QLabel* favorites_label_ = nullptr;
    QLabel* deleted_label_ = nullptr;
    QLabel* size_label_ = nullptr;

    static QString format_size(uint64_t bytes);
};
