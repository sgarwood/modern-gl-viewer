using GameBackend.Models;

namespace GameBackend.Services;

/// <summary>
/// The elevation, the weather and the sun times over a point on the earth.
/// </summary>
public interface IConditionsProvider
{
    Task<SiteConditions?> FetchAsync(
        string name,
        double latitude,
        double longitude,
        CancellationToken cancellationToken);
}
