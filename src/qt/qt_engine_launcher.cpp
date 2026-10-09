#include "qt_engine_launcher.hpp"

#include "qt_viewer_window.hpp"

#include <utility>

QtEngineLauncher::QtEngineLauncher() = default;
QtEngineLauncher::~QtEngineLauncher() = default;

void QtEngineLauncher::launch(mgv::AssetPaths assets, mgv::CourseConditions conditions) {
    if (!window_) {
        window_ = std::make_unique<QtViewerWindow>(std::move(assets), std::move(conditions));
    } else {
        window_->load_assets(std::move(assets), std::move(conditions));
    }
    window_->show();
    window_->raise();
    window_->activateWindow();
}
