using GameBackend.Models;

namespace GameBackend.Services;

/// <summary>
/// Somewhere to look up golf clubs by name or by the place they are near.
/// </summary>
/// <remarks>
/// A port, so the provider can change without the endpoint noticing. Today it
/// is OpenStreetMap's Nominatim, which needs no key and whose data for UK
/// golf clubs is good because clubs tag themselves. A keyed service such as
/// Google Places would implement the same interface and read its key from
/// configuration here, never from the client.
/// </remarks>
public interface IClubDirectory
{
    Task<IReadOnlyList<Club>> SearchAsync(string query, CancellationToken cancellationToken);
}
