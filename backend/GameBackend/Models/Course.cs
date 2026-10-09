namespace GameBackend.Models;

public class Course
{
    public int Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public string Location { get; set; } = string.Empty;
    public int Par { get; set; }

    /// <summary>
    /// Where the course is. Weather and sun position are both looked up from
    /// it, so a course without coordinates cannot be played in real
    /// conditions.
    /// </summary>
    public double Latitude { get; set; }

    public double Longitude { get; set; }
}
