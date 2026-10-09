using GameBackend.Models;
using Microsoft.EntityFrameworkCore;

namespace GameBackend.Data;

public class GameDbContext : DbContext
{
    public GameDbContext(DbContextOptions<GameDbContext> options) : base(options) { }

    public DbSet<Course> Courses => Set<Course>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        // Seed some initial courses. Coordinates are the clubhouses: weather
        // and sun position are looked up from them, so a seeded course
        // without them would have played at latitude zero, longitude zero,
        // in the Gulf of Guinea.
        modelBuilder.Entity<Course>().HasData(
            new Course
            {
                Id = 1, Name = "St Andrews Links", Location = "Scotland", Par = 72,
                Latitude = 56.3433, Longitude = -2.8030,
            },
            new Course
            {
                Id = 2, Name = "Pebble Beach", Location = "California", Par = 72,
                Latitude = 36.5686, Longitude = -121.9500,
            },
            new Course
            {
                Id = 3, Name = "Augusta National", Location = "Georgia", Par = 72,
                Latitude = 33.5021, Longitude = -82.0220,
            }
        );
    }
}
