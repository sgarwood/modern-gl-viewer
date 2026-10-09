using FluentValidation;

namespace GameBackend.Validation;

/// <summary>
/// A club search needs something to search for, and a bounded amount of it.
/// </summary>
public class ClubQueryValidator : AbstractValidator<string>
{
    public ClubQueryValidator()
    {
        RuleFor(query => query)
            .NotEmpty().WithMessage("A search needs a club name or a place.")
            .MinimumLength(3).WithMessage("Search for at least three characters.")
            .MaximumLength(120).WithMessage("That is too long to be a club name.");
    }
}
