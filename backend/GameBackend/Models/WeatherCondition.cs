namespace GameBackend.Models;

public class WeatherCondition
{
    public double TemperatureC { get; set; }

    public double WindSpeedMps { get; set; }

    /// <summary>
    /// The compass bearing the wind blows <em>towards</em>, which is what the
    /// engine's weather command wants.
    /// </summary>
    /// <remarks>
    /// Note this is the opposite of the meteorological convention every
    /// weather service reports in, where a "westerly" is wind arriving from
    /// the west. Providers convert on the way in; getting it backwards puts
    /// every crosswind on the wrong side of the fairway.
    /// </remarks>
    public double WindDirectionDeg { get; set; }

    public bool IsRaining { get; set; }

    /// <summary>How wet the turf is, 0 to 1.</summary>
    public double TurfWetness { get; set; }

    /// <summary>Station pressure in pascals, already adjusted for elevation.</summary>
    public double PressurePascals { get; set; } = 101_325.0;

    /// <summary>Relative humidity, 0 to 1.</summary>
    public double RelativeHumidity { get; set; }
}
