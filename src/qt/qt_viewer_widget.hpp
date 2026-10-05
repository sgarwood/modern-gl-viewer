#pragma once

#include "mgv/engine.hpp"

#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <exception>
#include <memory>

class QKeyEvent;

class QtViewerWidget final : public QOpenGLWidget {
public:
    explicit QtViewerWidget(mgv::AssetPaths assets, QWidget* parent = nullptr);
    ~QtViewerWidget() override;

    QtViewerWidget(const QtViewerWidget&) = delete;
    QtViewerWidget& operator=(const QtViewerWidget&) = delete;

    void load_assets(mgv::AssetPaths assets);
    void handle_input(mgv::InputAction action);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void initializeGL() override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void cleanup();
    void show_error(const std::exception& error);

    mgv::AssetPaths assets_;
    std::unique_ptr<mgv::Engine> engine_;
    QTimer animation_timer_;
    QString error_;
};
