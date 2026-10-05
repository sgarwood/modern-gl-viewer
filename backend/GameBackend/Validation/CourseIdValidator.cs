using FluentValidation;

namespace GameBackend.Validation;

public class CourseIdValidator : AbstractValidator<int>
{
    public CourseIdValidator()
    {
        RuleFor(id => id)
            .GreaterThan(0).WithMessage("Course ID must be greater than zero.");
    }
}
