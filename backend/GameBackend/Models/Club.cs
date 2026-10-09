namespace GameBackend.Models;

/// <summary>
/// A golf club found by searching, and where it is.
/// </summary>
public class Club
{
    /// <summary>The provider's own identifier, so a client can ask again without re-searching.</summary>
    public long Id { get; set; }

    public string Name { get; set; } = string.Empty;

    /// <summary>The full address the provider gave, for telling two clubs of the same name apart.</summary>
    public string Address { get; set; } = string.Empty;

    public double Latitude { get; set; }

    public double Longitude { get; set; }
}
