using FluentValidation;
using GameBackend.Data;
using GameBackend.Models;
using GameBackend.Services;
using GameBackend.Validation;
using Microsoft.EntityFrameworkCore;

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

// Configure Entity Framework with SQLite
builder.Services.AddDbContext<GameDbContext>(options =>
    options.UseSqlite("Data Source=game.db"));

// Register FluentValidation
builder.Services.AddValidatorsFromAssemblyContaining<CourseIdValidator>();

// Upstream providers. Both are keyless, and both are reached only from here:
// the game client never talks to them directly, so a keyed provider can
// replace either without a client release.
builder.Services.AddHttpClient<IClubDirectory, NominatimClubDirectory>(client =>
{
    client.BaseAddress = new Uri("https://nominatim.openstreetmap.org/");
    // Nominatim's usage policy requires an identifying User-Agent and will
    // refuse requests without one.
    client.DefaultRequestHeaders.UserAgent.ParseAdd("hardcore-golf-backend/0.1");
    client.Timeout = TimeSpan.FromSeconds(10);
});

builder.Services.AddHttpClient<IConditionsProvider, OpenMeteoConditions>(client =>
{
    client.BaseAddress = new Uri("https://api.open-meteo.com/");
    client.Timeout = TimeSpan.FromSeconds(10);
});

var app = builder.Build();

// Ensure Database is created and seeded
using (var scope = app.Services.CreateScope())
{
    var db = scope.ServiceProvider.GetRequiredService<GameDbContext>();
    db.Database.EnsureCreated();
}

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}
else
{
    // Not in development: the game's HTTP client is cpp-httplib built without
    // TLS, so a redirect to HTTPS is a request it cannot follow. Over a
    // loopback dev backend there is nothing to protect anyway.
    app.UseHttpsRedirection();
}

// GET /courses -> List all golf courses
app.MapGet("/courses", async (GameDbContext db) =>
{
    var courses = await db.Courses.ToListAsync();
    return Results.Ok(courses);
})
.WithName("GetCourses")
.WithOpenApi();

// GET /clubs?query=... -> Search UK golf clubs by name or by a nearby place
app.MapGet("/clubs", async (
    string? query,
    IClubDirectory directory,
    IValidator<string> validator,
    CancellationToken cancellationToken) =>
{
    var validationResult = await validator.ValidateAsync(query ?? string.Empty, cancellationToken);
    if (!validationResult.IsValid)
    {
        return Results.ValidationProblem(validationResult.ToDictionary());
    }

    var clubs = await directory.SearchAsync(query!, cancellationToken);
    return Results.Ok(clubs);
})
.WithName("SearchClubs")
.WithOpenApi();

// GET /conditions?latitude=..&longitude=..&name=.. -> Everything the game
// needs to place a round in the world: elevation, weather, and sun times.
app.MapGet("/conditions", async (
    double latitude,
    double longitude,
    string? name,
    IConditionsProvider provider,
    CancellationToken cancellationToken) =>
{
    if (latitude is < -90.0 or > 90.0 || longitude is < -180.0 or > 180.0)
    {
        return Results.BadRequest(new { Message = "Latitude or longitude is off the earth." });
    }

    var conditions = await provider.FetchAsync(
        name ?? "Unnamed course", latitude, longitude, cancellationToken);

    // No conditions rather than invented ones: a client that is told the
    // request failed falls back to Greenwich at midday, which is a defensible
    // place to play. One handed plausible numbers from a dead service cannot
    // tell that anything is wrong.
    return conditions is null
        ? Results.Problem("The weather service did not answer.", statusCode: 503)
        : Results.Ok(conditions);
})
.WithName("GetConditions")
.WithOpenApi();

// GET /courses/{id}/weather -> Live weather over a stored course
app.MapGet("/courses/{id:int}/weather", async (
    int id,
    GameDbContext db,
    IValidator<int> validator,
    IConditionsProvider provider,
    CancellationToken cancellationToken) =>
{
    var validationResult = await validator.ValidateAsync(id, cancellationToken);
    if (!validationResult.IsValid)
    {
        return Results.ValidationProblem(validationResult.ToDictionary());
    }

    var course = await db.Courses.FindAsync(new object?[] { id }, cancellationToken);
    if (course is null)
    {
        return Results.NotFound(new { Message = $"Course with ID {id} not found." });
    }

    // Was a deterministic random number generator seeded on the course id and
    // the day of the year, which produced a plausible forecast that had
    // nothing to do with the weather. The course carries coordinates now, so
    // this can be the real thing.
    var conditions = await provider.FetchAsync(
        course.Name, course.Latitude, course.Longitude, cancellationToken);

    return conditions is null
        ? Results.Problem("The weather service did not answer.", statusCode: 503)
        : Results.Ok(conditions.Weather);
})
.WithName("GetCourseWeather")
.WithOpenApi();

app.Run();
