#include "qt_viewer_widget.hpp"

#include "mgv/opengl_backend.hpp"

#include <QByteArray>
#include <QColor>
#include <QOpenGLContext>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

QtViewerWidget::QtViewerWidget(mgv::AssetPaths assets, QWidget* parent)
    : QOpenGLWidget{parent}, assets_{std::move(assets)} {
    animation_timer_.setInterval(16);
    animation_timer_.setTimerType(Qt::PreciseTimer);
    connect(&animation_timer_, &QTimer::timeout, this, [this] { update(); });
}

QtViewerWidget::~QtViewerWidget() {
    animation_timer_.stop();
    cleanup();
}

void QtViewerWidget::load_assets(mgv::AssetPaths assets) {
    assets_ = std::move(assets);
    if (!renderer_) {
        return;
    }

    makeCurrent();
    try {
        renderer_->load(assets_);
        error_.clear();
        if (!elapsed_.isValid()) {
            elapsed_.start();
        }
        animation_timer_.start();
    } catch (const std::exception& error) {
        show_error(error);
    }
    doneCurrent();
    update();
}

QSize QtViewerWidget::sizeHint() const {
    return {1280, 720};
}

void QtViewerWidget::initializeGL() {
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] { cleanup(); }, Qt::DirectConnection);

    try {
        renderer_ = std::make_unique<mgv::Renderer>(mgv::make_opengl_backend([](const char* name) {
            const auto* current = QOpenGLContext::currentContext();
            return current == nullptr ? nullptr : current->getProcAddress(QByteArray{name});
        }));
        renderer_->load(assets_);
        error_.clear();
        elapsed_.start();
        animation_timer_.start();
    } catch (const std::exception& error) {
        show_error(error);
    }
}

void QtViewerWidget::paintGL() {
    if (renderer_ && error_.isEmpty()) {
        try {
            const auto scale = devicePixelRatioF();
            renderer_->render({
                .framebuffer_width = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * scale))),
                .framebuffer_height = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * scale))),
                .elapsed_seconds = static_cast<float>(elapsed_.elapsed()) / 1000.0F,
            });
            return;
        } catch (const std::exception& error) {
            show_error(error);
        }
    }

    QPainter painter{this};
    painter.fillRect(rect(), QColor{"#111827"});
    painter.setPen(QColor{"#fca5a5"});
    painter.drawText(rect().adjusted(32, 32, -32, -32), Qt::AlignCenter | Qt::TextWordWrap, error_);
}

void QtViewerWidget::cleanup() {
    if (!renderer_) {
        return;
    }
    makeCurrent();
    renderer_.reset();
    doneCurrent();
}

void QtViewerWidget::show_error(const std::exception& error) {
    error_ = QString::fromUtf8(error.what());
    animation_timer_.stop();
}
