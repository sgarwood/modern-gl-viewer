#pragma once

#include "main_menu_controller.hpp"

#include <memory>

class QtViewerWindow;

class QtEngineLauncher final : public EngineLauncher {
public:
    QtEngineLauncher();
    ~QtEngineLauncher() override;

    QtEngineLauncher(const QtEngineLauncher&) = delete;
    QtEngineLauncher& operator=(const QtEngineLauncher&) = delete;

    void launch(mgv::AssetPaths assets, mgv::CourseConditions conditions) override;

private:
    std::unique_ptr<QtViewerWindow> window_;
};
