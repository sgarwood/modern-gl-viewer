using System.Globalization;
using System.Net.Http.Json;
using System.Text.Json.Serialization;
using GameBackend.Models;

namespace GameBackend.Services;

/// <summary>
/// UK golf clubs, from OpenStreetMap by way of Nominatim.
/// </summary>
public sealed class NominatimClubDirectory : IClubDirectory
{
    private const int ResultLimit = 10;

    private readonly HttpClient _client;
    private readonly ILogger<NominatimClubDirectory> _logger;

    public NominatimClubDirectory(HttpClient client, ILogger<NominatimClubDirectory> logger)
    {
        _client = client;
        _logger = logger;
    }

    public async Task<IReadOnlyList<Club>> SearchAsync(
        string query,
        CancellationToken cancellationToken)
    {
        // The [golf_course] special phrase restricts the search to things
        // tagged leisure=golf_course. Free text does not: a search for
        // "Wentworth Golf Club" returns Wath Golf Club, in Wentworth, because
        // the place name outweighs the club name.
        var search = Uri.EscapeDataString($"[golf_course] {query}");
        var path =
            $"search?q={search}&format=jsonv2&countrycodes=gb&addressdetails=1&limit={ResultLimit}";

        try
        {
            var places = await _client.GetFromJsonAsync<List<NominatimPlace>>(
                path, cancellationToken);
            if (places is null)
            {
                return Array.Empty<Club>();
            }

            return places
                // The special phrase is a hint rather than a filter, so the
                // tag is checked again here.
                .Where(place => place.Category == "leisure" && place.Type == "golf_course")
                .Select(ToClub)
                .Where(club => club is not null)
                .Select(club => club!)
                .ToList();
        }
        catch (Exception exception) when (exception is HttpRequestException or TaskCanceledException)
        {
            _logger.LogWarning(exception, "Club search for {Query} failed upstream", query);
            return Array.Empty<Club>();
        }
    }

    private static Club? ToClub(NominatimPlace place)
    {
        // Nominatim sends coordinates as strings, and as decimals with a dot
        // whatever the server's locale is, so they are parsed as invariant.
        if (!double.TryParse(place.Latitude, NumberStyles.Float, CultureInfo.InvariantCulture, out var latitude) ||
            !double.TryParse(place.Longitude, NumberStyles.Float, CultureInfo.InvariantCulture, out var longitude))
        {
            return null;
        }

        return new Club
        {
            Id = place.PlaceId,
            Name = string.IsNullOrWhiteSpace(place.Name) ? place.DisplayName ?? "Unnamed club" : place.Name,
            Address = place.DisplayName ?? string.Empty,
            Latitude = latitude,
            Longitude = longitude,
        };
    }

    private sealed record NominatimPlace(
        [property: JsonPropertyName("place_id")] long PlaceId,
        [property: JsonPropertyName("name")] string? Name,
        [property: JsonPropertyName("display_name")] string? DisplayName,
        [property: JsonPropertyName("lat")] string? Latitude,
        [property: JsonPropertyName("lon")] string? Longitude,
        [property: JsonPropertyName("category")] string? Category,
        [property: JsonPropertyName("type")] string? Type);
}
