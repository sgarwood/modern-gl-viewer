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

QtViewerWidget::QtViewerWidget(
    mgv::AssetPaths assets,
    mgv::CourseConditions conditions,
    QWidget* parent)
    : QOpenGLWidget{parent},
      assets_{std::move(assets)},
      conditions_{std::move(conditions)} {
    setFocusPolicy(Qt::StrongFocus);
    animation_timer_.setInterval(16);
    animation_timer_.setTimerType(Qt::PreciseTimer);
    connect(&animation_timer_, &QTimer::timeout, this, [this] { update(); });
}

QtViewerWidget::~QtViewerWidget() {
    animation_timer_.stop();
    cleanup();
}

void QtViewerWidget::load_assets(mgv::AssetPaths assets, mgv::CourseConditions conditions) {
    assets_ = std::move(assets);
    conditions_ = std::move(conditions);
    if (!engine_) {
        return;
    }

    makeCurrent();
    try {
        course_ = mgv::default_course_session(assets_.model.parent_path());
        course_.site = conditions_.site;
        session_ = mgv::configure_course_session(*engine_, course_);
        apply_conditions();
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
    // The range finder is a matter for the round, not the view.
    if (action == mgv::InputAction::fire_test_shot) {
        mgv::play_test_shot(*engine_, session_);
    } else if (action == mgv::InputAction::toggle_range_finder) {
        static_cast<void>(mgv::toggle_range_finder(*engine_, session_));
    } else if (action == mgv::InputAction::range_finder_ping) {
        last_range_ = mgv::range_find(*engine_, session_);
    } else {
        engine_->enqueue(action);
    }
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
        course_ = mgv::default_course_session(assets_.model.parent_path());
        course_.site = conditions_.site;
        session_ = mgv::configure_course_session(*engine_, course_);
        apply_conditions();
        error_.clear();
        animation_timer_.start();
    } catch (const std::exception& error) {
        show_error(error);
    }
}

void QtViewerWidget::paintGL() {
    if (engine_ && error_.isEmpty()) {
        try {
            // Detail follows the camera, so this has to run before the frame
            // that uses it.
            static_cast<void>(mgv::stream_course(*engine_, session_, course_));
            static_cast<void>(mgv::advance_round(*engine_, session_, course_));
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

void QtViewerWidget::apply_conditions() {
    // configure_course_session has already set the air from the site's
    // elevation. This replaces it with what a service actually measured over
    // the course, which knows what the weather is doing as well as how high
    // up it is.
    engine_->enqueue(mgv::SetWeatherCommand{
        .temperature_c = conditions_.weather.temperature_c,
        .wind_speed_mps = conditions_.weather.wind_speed_mps,
        .wind_direction_deg = conditions_.weather.wind_direction_deg,
        .pressure_pa = conditions_.weather.pressure_pascals,
        .relative_humidity = conditions_.weather.relative_humidity,
        .turf_wetness = conditions_.weather.turf_wetness,
    });
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
