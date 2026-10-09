using System.Globalization;
using System.Net.Http.Json;
using System.Text.Json.Serialization;
using GameBackend.Models;

namespace GameBackend.Services;

/// <summary>
/// Conditions from Open-Meteo, which needs no key and answers all of it at
/// once: terrain elevation, the current hour's weather, and the day's
/// sunrise and sunset.
/// </summary>
public sealed class OpenMeteoConditions : IConditionsProvider
{
    private readonly HttpClient _client;
    private readonly ILogger<OpenMeteoConditions> _logger;

    public OpenMeteoConditions(HttpClient client, ILogger<OpenMeteoConditions> logger)
    {
        _client = client;
        _logger = logger;
    }

    public async Task<SiteConditions?> FetchAsync(
        string name,
        double latitude,
        double longitude,
        CancellationToken cancellationToken)
    {
        // GMT rather than the local zone, because the game works in UTC and
        // converting twice is how an hour goes missing. Metres per second
        // rather than km/h for the same reason.
        var path =
            "v1/forecast" +
            $"?latitude={latitude.ToString("0.####", CultureInfo.InvariantCulture)}" +
            $"&longitude={longitude.ToString("0.####", CultureInfo.InvariantCulture)}" +
            "&current=temperature_2m,relative_humidity_2m,precipitation,rain," +
            "surface_pressure,wind_speed_10m,wind_direction_10m" +
            "&daily=sunrise,sunset&timezone=GMT&wind_speed_unit=ms&forecast_days=1";

        try
        {
            var response = await _client.GetFromJsonAsync<OpenMeteoForecast>(path, cancellationToken);
            if (response?.Current is null)
            {
                return null;
            }

            var current = response.Current;
            var observed = ParseIsoTime(current.Time);

            return new SiteConditions
            {
                Name = name,
                Latitude = response.Latitude,
                Longitude = response.Longitude,
                ElevationMetres = response.Elevation,
                DayOfYear = observed?.DayOfYear ?? DateTime.UtcNow.DayOfYear,
                UtcHours = observed is null
                    ? DateTime.UtcNow.TimeOfDay.TotalHours
                    : observed.Value.TimeOfDay.TotalHours,
                SunriseUtcHours = HoursOf(response.Daily?.Sunrise) ?? 0.0,
                SunsetUtcHours = HoursOf(response.Daily?.Sunset) ?? 0.0,
                Weather = new WeatherCondition
                {
                    TemperatureC = current.Temperature,
                    WindSpeedMps = current.WindSpeed,
                    // Open-Meteo reports the direction the wind comes FROM,
                    // as every weather service does. The engine wants the
                    // bearing it blows TOWARDS, so this turns through 180.
                    // Without it every crosswind pushes the ball to the wrong
                    // side of the fairway.
                    WindDirectionDeg = (current.WindDirection + 180.0) % 360.0,
                    IsRaining = current.Rain > 0.0,
                    TurfWetness = TurfWetness(current.Rain, current.RelativeHumidity),
                    // Reported in hectopascals; the engine works in pascals.
                    PressurePascals = current.SurfacePressure * 100.0,
                    RelativeHumidity = Math.Clamp(current.RelativeHumidity / 100.0, 0.0, 1.0),
                },
            };
        }
        catch (Exception exception) when (exception is HttpRequestException or TaskCanceledException)
        {
            _logger.LogWarning(
                exception, "Conditions for {Latitude},{Longitude} failed upstream", latitude, longitude);
            return null;
        }
    }

    /// <summary>
    /// How wet the turf is, from the rain falling on it and the humidity over
    /// it.
    /// </summary>
    /// <remarks>
    /// A judgement, not a measurement: no service reports turf wetness. Rain
    /// dominates when there is any, and humidity stands in for dew when there
    /// is not, which is why a British green is slow at eight in the morning
    /// and quick by two in the afternoon. The thresholds are chosen so a dry
    /// summer afternoon reads zero.
    /// </remarks>
    private static double TurfWetness(double rainMillimetres, double relativeHumidityPercent)
    {
        if (rainMillimetres > 0.0)
        {
            return Math.Clamp(0.5 + (rainMillimetres / 4.0), 0.0, 1.0);
        }

        return Math.Clamp((relativeHumidityPercent - 60.0) / 80.0, 0.0, 1.0);
    }

    private static double? HoursOf(string[]? times)
    {
        if (times is null || times.Length == 0)
        {
            return null;
        }

        var parsed = ParseIsoTime(times[0]);
        return parsed?.TimeOfDay.TotalHours;
    }

    private static DateTime? ParseIsoTime(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return null;
        }

        return DateTime.TryParse(
            value,
            CultureInfo.InvariantCulture,
            DateTimeStyles.None,
            out var parsed)
            ? parsed
            : null;
    }

    private sealed record OpenMeteoForecast(
        [property: JsonPropertyName("latitude")] double Latitude,
        [property: JsonPropertyName("longitude")] double Longitude,
        [property: JsonPropertyName("elevation")] double Elevation,
        [property: JsonPropertyName("current")] OpenMeteoCurrent? Current,
        [property: JsonPropertyName("daily")] OpenMeteoDaily? Daily);

    private sealed record OpenMeteoCurrent(
        [property: JsonPropertyName("time")] string? Time,
        [property: JsonPropertyName("temperature_2m")] double Temperature,
        [property: JsonPropertyName("relative_humidity_2m")] double RelativeHumidity,
        [property: JsonPropertyName("precipitation")] double Precipitation,
        [property: JsonPropertyName("rain")] double Rain,
        [property: JsonPropertyName("surface_pressure")] double SurfacePressure,
        [property: JsonPropertyName("wind_speed_10m")] double WindSpeed,
        [property: JsonPropertyName("wind_direction_10m")] double WindDirection);

    private sealed record OpenMeteoDaily(
        [property: JsonPropertyName("sunrise")] string[]? Sunrise,
        [property: JsonPropertyName("sunset")] string[]? Sunset);
}
