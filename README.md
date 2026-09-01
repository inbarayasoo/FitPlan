# FitPlan

A workout-tracking platform with a **coach** side and a **trainee** side, built as a
C++20 REST API. Coaches maintain an exercise library, build workout plans (with an
optional tutorial video per exercise), assign them to trainees, and track progress.
Trainees follow their assigned plan, log each session, and see their personal
records and trends.

> **Project status:** under active development, built step by step. See
> [`docs/`](docs/) and the roadmap below for what is done and what is planned.

## Tech stack

| Concern        | Choice                                             |
| -------------- | -------------------------------------------------- |
| Language       | C++20                                              |
| Build          | CMake (+ Ninja), `FetchContent` for dependencies   |
| HTTP framework | [Crow](https://github.com/CrowCpp/Crow)            |
| Database       | SQLite via [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) |
| Auth           | JWT (HS256) + Argon2id password hashing (libsodium)|
| JSON           | [nlohmann/json](https://github.com/nlohmann/json)  |
| Logging        | [spdlog](https://github.com/gabime/spdlog)         |
| Tests          | GoogleTest (unit + integration)                    |
| API docs       | OpenAPI 3 + Swagger UI at `/docs`                  |
| Packaging      | Docker + docker-compose                            |
| CI             | GitHub Actions (build, tests, formatting, image)   |

## Architecture

Layered, with a one-way dependency flow:

```
HTTP request
   -> Controller   (parse & validate DTOs, shape responses)
   -> Service      (business rules, authorization decisions)
   -> Repository    (SQL against SQLite)
   -> Database
```

Cross-cutting concerns (JWT authentication, request logging, uniform
`application/problem+json` errors) live in middleware.

## Getting started

The project targets a Linux toolchain (developed on WSL2 / Ubuntu 24.04).

### Prerequisites

```bash
sudo apt install -y build-essential g++-13 cmake ninja-build git pkg-config \
    libssl-dev libsodium-dev libsqlite3-dev libasio-dev
```

### Build and run

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/fitplan
```

Then, in another terminal:

```bash
curl -s http://localhost:8080/api/health
# {"service":"FitPlan","status":"ok","version":"0.1.0"}
```

### Run the tests

```bash
ctest --preset dev
```

### Seed data

Load a small sample dataset (one coach, one trainee, an exercise library, an
active plan, and two logged sessions):

```bash
./build/dev/fitplan --seed   # run from the repo root
```

Seeded logins:

| Role    | Email                 | Password            |
| ------- | --------------------- | ------------------- |
| Coach   | `coach@fitplan.dev`   | `coach-demo-pass`   |
| Trainee | `trainee@fitplan.dev` | `trainee-demo-pass` |

### Configuration

All configuration is via environment variables (safe defaults for development):

| Variable                  | Default                        | Purpose                       |
| ------------------------- | ------------------------------ | ----------------------------- |
| `FITPLAN_HOST`            | `0.0.0.0`                      | Bind address                  |
| `FITPLAN_PORT`            | `8080`                        | TCP port                      |
| `FITPLAN_DB_PATH`         | `fitplan.db`                   | SQLite file path              |
| `FITPLAN_JWT_SECRET`      | *(insecure dev value)*         | HMAC signing secret           |
| `FITPLAN_JWT_TTL_SECONDS` | `86400`                       | Access-token lifetime         |
| `FITPLAN_LOG_LEVEL`       | `info`                         | `trace`..`error`, or `off`    |
| `FITPLAN_THREADS`         | `0` (auto)                     | Worker thread count           |

## Project layout

```
src/
  main.cpp            process entry point
  config/             environment-driven configuration
  db/                 connection + migration runner
  models/             plain data structs
  repositories/       SQL data access
  services/           business logic
  controllers/        HTTP layer
  middleware/         auth, logging, error handling
tests/
  unit/               fast, pure-logic tests
  integration/        HTTP-level tests against a temp DB
cmake/                dependency and version-header helpers
docs/                 OpenAPI spec, architecture notes
web/                  static frontend
```

## Roadmap

- [x] Step 1 - Project skeleton, build system, `GET /api/health`
- [x] Step 2 - Database layer, migrations, seed data
- [x] Step 3 - Auth (register / login / JWT) + middleware
- [x] Step 4 - Coach API (exercises, plans, trainees, tutorial-video links)
- [x] Step 5 - Trainee API + progress engine (e1RM, volume, adherence, streak)
- [x] Step 6 - Web frontend
- [ ] Step 7 - Test hardening + GitHub Actions CI
- [ ] Step 8 - Docker, OpenAPI/Swagger, documentation polish

Later: refresh tokens, body-measurement log, coach/trainee messaging, calendar view.

## License

MIT - see [LICENSE](LICENSE).
