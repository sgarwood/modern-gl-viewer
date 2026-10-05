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
        
        // Seed some initial courses
        modelBuilder.Entity<Course>().HasData(
            new Course { Id = 1, Name = "St Andrews Links", Location = "Scotland", Par = 72 },
            new Course { Id = 2, Name = "Pebble Beach", Location = "California", Par = 72 },
            new Course { Id = 3, Name = "Augusta National", Location = "Georgia", Par = 72 }
        );
    }
}
