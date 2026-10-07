#include "qt_viewer_widget.hpp"

#include "mgv/course_session.hpp"
#include "mgv/opengl_backend.hpp"

#include <QByteArray>
#include <QColor>
#include <QKeyEvent>
#include <QOpenGLContext>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

namespace {

void configure_golf_session(mgv::Engine& engine, const mgv::AssetPaths& assets) {
    const auto description = mgv::default_course_session(assets.model.parent_path());
    static_cast<void>(mgv::configure_course_session(engine, description));
}

} // namespace

QtViewerWidget::QtViewerWidget(mgv::AssetPaths assets, QWidget* parent)
    : QOpenGLWidget{parent}, assets_{std::move(assets)} {
    setFocusPolicy(Qt::StrongFocus);
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
    if (!engine_) {
        return;
    }

    makeCurrent();
    try {
        configure_golf_session(*engine_, assets_);
        error_.clear();
        animation_timer_.start();
    } catch (const std::exception& error) {
        show_error(error);
    }
    doneCurrent();
    update();
}

void QtViewerWidget::handle_input(mgv::InputAction action) {
    if (!engine_) {
        return;
    }
    engine_->enqueue(action);
    update();
}

QSize QtViewerWidget::sizeHint() const {
    return {1280, 720};
}

void QtViewerWidget::initializeGL() {
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] { cleanup(); }, Qt::DirectConnection);

    try {
        engine_ = std::make_unique<mgv::Engine>(mgv::make_opengl_backend([](const char* name) {
            const auto* current = QOpenGLContext::currentContext();
            return current == nullptr ? nullptr : current->getProcAddress(QByteArray{name});
        }));
        configure_golf_session(*engine_, assets_);
        error_.clear();
        animation_timer_.start();
    } catch (const std::exception& error) {
        show_error(error);
    }
}

void QtViewerWidget::paintGL() {
    if (engine_ && error_.isEmpty()) {
        try {
            const auto scale = devicePixelRatioF();
            engine_->tick({
                .framebuffer_width = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * scale))),
                .framebuffer_height = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * scale))),
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

void QtViewerWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        handle_input(mgv::InputAction::orbit_left);
        break;
    case Qt::Key_Right:
        handle_input(mgv::InputAction::orbit_right);
        break;
    case Qt::Key_Up:
        handle_input(mgv::InputAction::orbit_up);
        break;
    case Qt::Key_Down:
        handle_input(mgv::InputAction::orbit_down);
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        handle_input(mgv::InputAction::zoom_in);
        break;
    case Qt::Key_Minus:
        handle_input(mgv::InputAction::zoom_out);
        break;
    case Qt::Key_Home:
    case Qt::Key_R:
        handle_input(mgv::InputAction::reset_view);
        break;
    case Qt::Key_Space:
        handle_input(mgv::InputAction::fire_test_shot);
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void QtViewerWidget::cleanup() {
    if (!engine_) {
        return;
    }
    makeCurrent();
    engine_.reset();
    doneCurrent();
}

void QtViewerWidget::show_error(const std::exception& error) {
    error_ = QString::fromUtf8(error.what());
    animation_timer_.stop();
}
