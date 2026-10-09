#include "main_menu_controller.hpp"

#include "mgv/solar.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

/// An hour of the day as a clock reading.
[[nodiscard]] QString clock(float utc_hours) {
    if (!std::isfinite(utc_hours)) {
        return QStringLiteral("--:--");
    }
    const auto total_minutes = static_cast<int>(std::lround(utc_hours * 60.0F));
    const auto hours = ((total_minutes / 60) % 24 + 24) % 24;
    const auto minutes = ((total_minutes % 60) + 60) % 60;
    return QStringLiteral("%1:%2")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'));
}

/// A latitude or longitude with its hemisphere, which is less work to read
/// than a signed number.
[[nodiscard]] QString coordinate(double degrees, QChar positive, QChar negative) {
    return QStringLiteral("%1\u00B0%2")
        .arg(std::abs(degrees), 0, 'f', 3)
        .arg(degrees >= 0.0 ? positive : negative);
}

} // namespace

MainMenuController::MainMenuController(
    EngineLauncher& launcher,
    mgv::AssetPaths default_assets,
    ClubDirectory* clubs,
    QObject* parent)
    : QObject{parent},
      launcher_{launcher},
      clubs_{clubs},
      default_assets_{std::move(default_assets)},
      model_source_{url_from(default_assets_.model)},
      vertex_shader_source_{url_from(default_assets_.vertex_shader)},
      fragment_shader_source_{url_from(default_assets_.fragment_shader)} {}

QUrl MainMenuController::modelSource() const { return model_source_; }
QUrl MainMenuController::vertexShaderSource() const { return vertex_shader_source_; }
QUrl MainMenuController::fragmentShaderSource() const { return fragment_shader_source_; }
const QString& MainMenuController::status() const noexcept { return status_; }
bool MainMenuController::hasError() const noexcept { return has_error_; }

void MainMenuController::setModelSource(const QUrl& value) {
    if (model_source_ == value) {
        return;
    }
    model_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::setVertexShaderSource(const QUrl& value) {
    if (vertex_shader_source_ == value) {
        return;
    }
    vertex_shader_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::setFragmentShaderSource(const QUrl& value) {
    if (fragment_shader_source_ == value) {
        return;
    }
    fragment_shader_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::launchEngine() {
    try {
        launcher_.launch(
            {
                .model = path_from(model_source_, "Model"),
                .vertex_shader = path_from(vertex_shader_source_, "Vertex shader"),
                .fragment_shader = path_from(fragment_shader_source_, "Fragment shader"),
            },
            conditions_);
        set_status("Engine loaded", false);
    } catch (const std::exception& error) {
        set_status(QString::fromUtf8(error.what()), true);
    }
}

void MainMenuController::restoreDefaults() {
    model_source_ = url_from(default_assets_.model);
    vertex_shader_source_ = url_from(default_assets_.vertex_shader);
    fragment_shader_source_ = url_from(default_assets_.fragment_shader);
    emit assetSourcesChanged();
    set_status("Bundled assets restored", false);
}

QUrl MainMenuController::url_from(const std::filesystem::path& value) {
    return QUrl::fromLocalFile(QString::fromStdString(value.string()));
}

std::filesystem::path MainMenuController::path_from(
    const QUrl& value,
    const char* label) {
    if (!value.isLocalFile() || value.toLocalFile().isEmpty()) {
        throw std::invalid_argument{std::string{label} + " must be a local file"};
    }
    return std::filesystem::path{value.toLocalFile().toStdString()};
}

void MainMenuController::set_status(QString status, bool error) {
    status_ = std::move(status);
    has_error_ = error;
    emit statusChanged();
}

const QString& MainMenuController::clubQuery() const noexcept { return club_query_; }
const QStringList& MainMenuController::clubResults() const noexcept { return club_results_; }
const QStringList& MainMenuController::clubAddresses() const noexcept { return club_addresses_; }
bool MainMenuController::hasClubDirectory() const noexcept { return clubs_ != nullptr; }
const mgv::CourseConditions& MainMenuController::conditions() const noexcept {
    return conditions_;
}

QString MainMenuController::siteName() const {
    return QString::fromStdString(conditions_.site.name);
}

QString MainMenuController::siteSummary() const {
    const auto& site = conditions_.site;
    return QStringLiteral("%1 %2  \u00B7  %3 m above sea level")
        .arg(coordinate(site.latitude_degrees, QLatin1Char('N'), QLatin1Char('S')))
        .arg(coordinate(site.longitude_degrees, QLatin1Char('E'), QLatin1Char('W')))
        .arg(static_cast<double>(site.elevation_metres), 0, 'f', 0);
}

QString MainMenuController::weatherSummary() const {
    const auto& weather = conditions_.weather;
    if (!site_resolved_) {
        // Said rather than shown as zeroes: a player looking at "0.0 m/s" has
        // no way to tell still air from no answer.
        return QStringLiteral("No live weather \u2014 standard still air");
    }
    return QStringLiteral("%1 \u00B0C  \u00B7  wind %2 m/s  \u00B7  turf %3% wet%4")
        .arg(static_cast<double>(weather.temperature_c), 0, 'f', 1)
        .arg(static_cast<double>(weather.wind_speed_mps), 0, 'f', 1)
        .arg(static_cast<double>(weather.turf_wetness) * 100.0, 0, 'f', 0)
        .arg(weather.is_raining ? QStringLiteral("  \u00B7  raining") : QString{});
}

QString MainMenuController::sunSummary() const {
    const auto& site = conditions_.site;
    const auto times = mgv::daylight_at(site);
    const auto sun = mgv::sun_at(site);
    if (!times.rises) {
        return QStringLiteral("The sun neither rises nor sets here today");
    }
    return QStringLiteral("Sunrise %1  \u00B7  sunset %2  \u00B7  sun %3\u00B0 up, bearing %4\u00B0")
        .arg(clock(times.sunrise_hours))
        .arg(clock(times.sunset_hours))
        .arg(static_cast<double>(sun.altitude_degrees), 0, 'f', 0)
        .arg(static_cast<double>(sun.azimuth_degrees), 0, 'f', 0);
}

void MainMenuController::setClubQuery(const QString& value) {
    if (club_query_ == value) {
        return;
    }
    club_query_ = value;
    emit clubQueryChanged();
}

void MainMenuController::searchClubs() {
    if (clubs_ == nullptr) {
        set_status("No course directory in this build", true);
        return;
    }
    const auto query = club_query_.trimmed();
    if (query.size() < 3) {
        set_status("Search for at least three characters", true);
        return;
    }

    found_ = clubs_->search(query.toStdString());
    club_results_.clear();
    club_addresses_.clear();
    for (const auto& club : found_) {
        club_results_.append(QString::fromStdString(club.name));
        club_addresses_.append(QString::fromStdString(club.address));
    }
    emit courseChanged();

    if (found_.empty()) {
        set_status(QStringLiteral("No golf clubs found for \u201C%1\u201D").arg(query), true);
    } else {
        set_status(
            QStringLiteral("%1 club%2 found")
                .arg(found_.size())
                .arg(found_.size() == 1 ? QString{} : QStringLiteral("s")),
            false);
    }
}

void MainMenuController::selectClub(int index) {
    if (clubs_ == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= found_.size()) {
        set_status("That club is not in the list", true);
        return;
    }

    const auto& club = found_[static_cast<std::size_t>(index)];
    const auto resolved = clubs_->conditions(club);
    if (!resolved) {
        // The club is known but the weather is not, so the place is kept and
        // the conditions are not invented. A player can still play it; they
        // are told the weather is standard.
        conditions_ = mgv::CourseConditions{};
        conditions_.site.name = club.name;
        conditions_.site.latitude_degrees = static_cast<float>(club.latitude);
        conditions_.site.longitude_degrees = static_cast<float>(club.longitude);
        site_resolved_ = false;
        emit courseChanged();
        set_status("Found the club, but no live conditions for it", true);
        return;
    }

    conditions_.site = mgv::network::course_site_of(*resolved);
    conditions_.weather = resolved->weather;
    site_resolved_ = true;
    emit courseChanged();
    set_status(QStringLiteral("Playing at %1").arg(QString::fromStdString(club.name)), false);
}

void MainMenuController::useGreenwich() {
    conditions_ = mgv::CourseConditions{};
    site_resolved_ = false;
    emit courseChanged();
    set_status("Playing at Greenwich, sea level, midday", false);
}
