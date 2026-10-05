namespace GameBackend.Models;

public class WeatherCondition
{
    public double TemperatureC { get; set; }
    public double WindSpeedMps { get; set; }
    public double WindDirectionDeg { get; set; }
    public bool IsRaining { get; set; }
    public double TurfWetness { get; set; }
}
