#pragma once

#include "mgv/renderer.hpp"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <exception>
#include <memory>

class QtViewerWidget final : public QOpenGLWidget {
public:
    explicit QtViewerWidget(mgv::AssetPaths assets, QWidget* parent = nullptr);
    ~QtViewerWidget() override;

    QtViewerWidget(const QtViewerWidget&) = delete;
    QtViewerWidget& operator=(const QtViewerWidget&) = delete;

    void load_assets(mgv::AssetPaths assets);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void cleanup();
    void show_error(const std::exception& error);

    mgv::AssetPaths assets_;
    std::unique_ptr<mgv::Renderer> renderer_;
    QElapsedTimer elapsed_;
    QTimer animation_timer_;
    QString error_;
};
