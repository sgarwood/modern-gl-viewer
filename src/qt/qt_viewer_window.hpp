#pragma once

#include "mgv/engine.hpp"

#include <QMainWindow>

class QtViewerWidget;

class QtViewerWindow final : public QMainWindow {
public:
    explicit QtViewerWindow(mgv::AssetPaths assets, QWidget* parent = nullptr);

    void load_assets(mgv::AssetPaths assets);

private:
    void create_actions();

    mgv::AssetPaths assets_;
    QtViewerWidget* viewer_{};
};
