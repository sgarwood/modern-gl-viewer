namespace GameBackend.Models;

/// <summary>
/// Everything the game needs to know about where and when a round is played:
/// the place, its height above the sea, the hour, and the weather over it.
/// </summary>
/// <remarks>
/// One object rather than three endpoints, because the client needs all of it
/// before it can render a single frame, and because the elevation and the
/// weather come from the same upstream call anyway.
/// </remarks>
public class SiteConditions
{
    public string Name { get; set; } = string.Empty;

    public double Latitude { get; set; }

    public double Longitude { get; set; }

    /// <summary>
    /// Metres above sea level, as the weather model's terrain has it. Air
    /// density follows from this, and carry distance follows from that.
    /// </summary>
    public double ElevationMetres { get; set; }

    /// <summary>Day of the year, 1 for the first of January.</summary>
    public int DayOfYear { get; set; }

    /// <summary>Hours UTC at the moment the conditions were observed.</summary>
    public double UtcHours { get; set; }

    public double SunriseUtcHours { get; set; }

    public double SunsetUtcHours { get; set; }

    public WeatherCondition Weather { get; set; } = new();
}
