using FluentValidation;
using GameBackend.Data;
using GameBackend.Models;
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

app.UseHttpsRedirection();

// GET /courses -> List all golf courses
app.MapGet("/courses", async (GameDbContext db) =>
{
    var courses = await db.Courses.ToListAsync();
    return Results.Ok(courses);
})
.WithName("GetCourses")
.WithOpenApi();

// GET /courses/{id}/weather -> Get weather for a specific course
app.MapGet("/courses/{id:int}/weather", async (int id, GameDbContext db, IValidator<int> validator) =>
{
    var validationResult = await validator.ValidateAsync(id);
    if (!validationResult.IsValid)
    {
        return Results.ValidationProblem(validationResult.ToDictionary());
    }

    var course = await db.Courses.FindAsync(id);
    if (course is null)
    {
        return Results.NotFound(new { Message = $"Course with ID {id} not found." });
    }

    // Mock live weather data based on the course location
    var rng = new Random(id + DateTime.UtcNow.DayOfYear); // Deterministic for the day per course
    
    var weather = new WeatherCondition
    {
        TemperatureC = rng.NextDouble() * 30.0 + 5.0, // 5 to 35 C
        WindSpeedMps = rng.NextDouble() * 15.0,       // 0 to 15 m/s
        WindDirectionDeg = rng.NextDouble() * 360.0,  // 0 to 360 deg
        IsRaining = rng.NextDouble() > 0.7,           // 30% chance of rain
    };
    
    // Turf wetness logic
    weather.TurfWetness = weather.IsRaining ? rng.NextDouble() * 0.5 + 0.5 : Math.Max(0, rng.NextDouble() - 0.5);

    return Results.Ok(weather);
})
.WithName("GetCourseWeather")
.WithOpenApi();

app.Run();
