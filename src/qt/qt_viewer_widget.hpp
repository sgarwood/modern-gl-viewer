#pragma once

#include "mgv/course_conditions.hpp"
#include "mgv/course_session.hpp"
#include "mgv/engine.hpp"

#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <exception>
#include <memory>
#include <optional>

class QKeyEvent;

class QtViewerWidget final : public QOpenGLWidget {
public:
    explicit QtViewerWidget(
        mgv::AssetPaths assets,
        mgv::CourseConditions conditions = {},
        QWidget* parent = nullptr);
    ~QtViewerWidget() override;

    QtViewerWidget(const QtViewerWidget&) = delete;
    QtViewerWidget& operator=(const QtViewerWidget&) = delete;

    void load_assets(mgv::AssetPaths assets, mgv::CourseConditions conditions = {});
    void handle_input(mgv::InputAction action);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void initializeGL() override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void cleanup();
    /// Puts the chosen place, hour and weather into the session.
    void apply_conditions();
    void show_error(const std::exception& error);

    mgv::AssetPaths assets_;
    mgv::CourseConditions conditions_;
    std::unique_ptr<mgv::Engine> engine_;
    mgv::CourseSessionDescription course_;
    mgv::CourseSession session_;
    /// The last yardage the range finder reported, in metres.
    std::optional<float> last_range_;
    QTimer animation_timer_;
    QString error_;
};
