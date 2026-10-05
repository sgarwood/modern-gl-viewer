# Development Guidelines

## 1. Strict Test-Driven Development (TDD)
*   Always write failing unit tests before implementing the logic.
*   Ensure every feature has both isolated unit tests and full-slice integration tests.

## 2. Commit As You Go
*   Do not stockpile massive changes.
*   Commit atomic units of work (e.g., interface creation, test creation, implementation) with clear, descriptive commit messages.

## 3. Complete The Vertical Slice
*   Always complete the current work before moving on to the next module.
*   A feature is not done until the interfaces are defined, the implementation is written, the unit/integration tests pass, and manual QA confirms its effectiveness.
*   Never leave dangling "TODOs" in active feature paths if they are part of the current vertical slice being built.

## 4. SOLID Design & Dependency Inversion
*   Strictly apply SOLID design principles where possible.
*   Abstract third-party libraries behind custom interfaces (Dependency Inversion). Never tightly couple concrete external implementations (like JSON parsers or specific network libraries) directly into the core business logic.

## 5. Manual QA & Evaluation
*   Beyond automated tests, proactively verify the system behaves correctly in the real environment.
*   Evaluate the effectiveness and performance of the solution before closing the task.
