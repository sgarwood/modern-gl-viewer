#pragma once

#include "mgv/course_conditions.hpp"
#include "mgv/engine.hpp"
#include "mgv/network/site_conditions.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class EngineLauncher {
public:
    virtual ~EngineLauncher() = default;
    EngineLauncher(const EngineLauncher&) = delete;
    EngineLauncher& operator=(const EngineLauncher&) = delete;

    virtual void launch(mgv::AssetPaths assets, mgv::CourseConditions conditions) = 0;

protected:
    EngineLauncher() = default;
};

/// Somewhere to look up golf clubs and the conditions over them.
///
/// A port, so the menu can be tested without a backend and so that the
/// transport can become asynchronous without the menu noticing. Both calls
/// block today, for up to the client's five second timeout.
class ClubDirectory {
public:
    virtual ~ClubDirectory() = default;
    ClubDirectory(const ClubDirectory&) = delete;
    ClubDirectory& operator=(const ClubDirectory&) = delete;

    /// Clubs matching a name or a place, nearest match first. Empty for no
    /// matches, and empty when the search failed.
    [[nodiscard]] virtual std::vector<mgv::network::Club> search(const std::string& query) = 0;

    /// The conditions over a club, or nothing when no service answered.
    [[nodiscard]] virtual std::optional<mgv::network::SiteConditions> conditions(
        const mgv::network::Club& club) = 0;

protected:
    ClubDirectory() = default;
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
    Q_PROPERTY(QString clubQuery READ clubQuery WRITE setClubQuery NOTIFY clubQueryChanged)
    Q_PROPERTY(QStringList clubResults READ clubResults NOTIFY courseChanged)
    Q_PROPERTY(QStringList clubAddresses READ clubAddresses NOTIFY courseChanged)
    Q_PROPERTY(QString siteName READ siteName NOTIFY courseChanged)
    Q_PROPERTY(QString siteSummary READ siteSummary NOTIFY courseChanged)
    Q_PROPERTY(QString weatherSummary READ weatherSummary NOTIFY courseChanged)
    Q_PROPERTY(QString sunSummary READ sunSummary NOTIFY courseChanged)
    Q_PROPERTY(bool hasClubDirectory READ hasClubDirectory CONSTANT)

public:
    MainMenuController(
        EngineLauncher& launcher,
        mgv::AssetPaths default_assets,
        ClubDirectory* clubs = nullptr,
        QObject* parent = nullptr);

    [[nodiscard]] QUrl modelSource() const;
    [[nodiscard]] QUrl vertexShaderSource() const;
    [[nodiscard]] QUrl fragmentShaderSource() const;
    [[nodiscard]] const QString& status() const noexcept;
    [[nodiscard]] bool hasError() const noexcept;

    void setModelSource(const QUrl& value);
    void setVertexShaderSource(const QUrl& value);
    void setFragmentShaderSource(const QUrl& value);

    [[nodiscard]] const QString& clubQuery() const noexcept;
    [[nodiscard]] const QStringList& clubResults() const noexcept;
    [[nodiscard]] const QStringList& clubAddresses() const noexcept;
    [[nodiscard]] QString siteName() const;
    [[nodiscard]] QString siteSummary() const;
    [[nodiscard]] QString weatherSummary() const;
    [[nodiscard]] QString sunSummary() const;
    [[nodiscard]] bool hasClubDirectory() const noexcept;
    /// What a launch would be played in. Greenwich at midday until a club has
    /// been chosen and a service has answered about it.
    [[nodiscard]] const mgv::CourseConditions& conditions() const noexcept;

    void setClubQuery(const QString& value);

    Q_INVOKABLE void launchEngine();
    Q_INVOKABLE void restoreDefaults();
    /// Searches for clubs matching `clubQuery`. Blocks.
    Q_INVOKABLE void searchClubs();
    /// Chooses the club at `index` of the last search and resolves the
    /// conditions over it.
    Q_INVOKABLE void selectClub(int index);
    /// Goes back to playing at Greenwich at midday.
    Q_INVOKABLE void useGreenwich();

signals:
    void assetSourcesChanged();
    void statusChanged();
    void clubQueryChanged();
    void courseChanged();

private:
    [[nodiscard]] static QUrl url_from(const std::filesystem::path& value);
    [[nodiscard]] static std::filesystem::path path_from(
        const QUrl& value,
        const char* label);
    void set_status(QString status, bool error);

    EngineLauncher& launcher_;
    ClubDirectory* clubs_{};
    mgv::AssetPaths default_assets_;
    QString club_query_;
    std::vector<mgv::network::Club> found_;
    QStringList club_results_;
    QStringList club_addresses_;
    mgv::CourseConditions conditions_{};
    bool site_resolved_{};
    QUrl model_source_;
    QUrl vertex_shader_source_;
    QUrl fragment_shader_source_;
    QString status_{"Ready"};
    bool has_error_{};
};
