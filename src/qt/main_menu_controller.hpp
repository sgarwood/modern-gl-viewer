#pragma once

#include "mgv/renderer.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

#include <filesystem>

class EngineLauncher {
public:
    virtual ~EngineLauncher() = default;
    EngineLauncher(const EngineLauncher&) = delete;
    EngineLauncher& operator=(const EngineLauncher&) = delete;

    virtual void launch(mgv::AssetPaths assets) = 0;

protected:
    EngineLauncher() = default;
};

class MainMenuController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl modelSource READ modelSource WRITE setModelSource NOTIFY assetSourcesChanged)
    Q_PROPERTY(
        QUrl vertexShaderSource READ vertexShaderSource WRITE setVertexShaderSource
        NOTIFY assetSourcesChanged)
    Q_PROPERTY(
        QUrl fragmentShaderSource READ fragmentShaderSource WRITE setFragmentShaderSource
        NOTIFY assetSourcesChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY statusChanged)

public:
    MainMenuController(
        EngineLauncher& launcher,
        mgv::AssetPaths default_assets,
        QObject* parent = nullptr);

    [[nodiscard]] QUrl modelSource() const;
    [[nodiscard]] QUrl vertexShaderSource() const;
    [[nodiscard]] QUrl fragmentShaderSource() const;
    [[nodiscard]] const QString& status() const noexcept;
    [[nodiscard]] bool hasError() const noexcept;

    void setModelSource(const QUrl& value);
    void setVertexShaderSource(const QUrl& value);
    void setFragmentShaderSource(const QUrl& value);

    Q_INVOKABLE void launchEngine();
    Q_INVOKABLE void restoreDefaults();

signals:
    void assetSourcesChanged();
    void statusChanged();

private:
    [[nodiscard]] static QUrl url_from(const std::filesystem::path& value);
    [[nodiscard]] static std::filesystem::path path_from(
        const QUrl& value,
        const char* label);
    void set_status(QString status, bool error);

    EngineLauncher& launcher_;
    mgv::AssetPaths default_assets_;
    QUrl model_source_;
    QUrl vertex_shader_source_;
    QUrl fragment_shader_source_;
    QString status_{"Ready"};
    bool has_error_{};
};
