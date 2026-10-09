#pragma once

#include "mgv/course_conditions.hpp"
#include "mgv/engine.hpp"

#include <QMainWindow>

class QtViewerWidget;

class QtViewerWindow final : public QMainWindow {
public:
    explicit QtViewerWindow(
        mgv::AssetPaths assets,
        mgv::CourseConditions conditions = {},
        QWidget* parent = nullptr);

    void load_assets(mgv::AssetPaths assets, mgv::CourseConditions conditions = {});

private:
    void create_actions();

    mgv::AssetPaths assets_;
    mgv::CourseConditions conditions_;
    QtViewerWidget* viewer_{};
};
