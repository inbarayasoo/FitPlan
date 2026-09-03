# syntax=docker/dockerfile:1

# ---------------------------------------------------------------------------
# Stage 1 "build" — full toolchain, compiles the release binary.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

# Toolchain + the -dev libraries the project links against. git + ca-certificates
# are needed because CMake FetchContent clones Crow, spdlog, SQLiteCpp, jwt-cpp.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++-13 cmake ninja-build pkg-config git ca-certificates \
        libssl-dev libsodium-dev libsqlite3-dev libasio-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# CMake checks that every source file listed in CMakeLists.txt exists at
# configure time, so the build definition and src/ must both be present before
# the first cmake call.
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake/ ./cmake/
COPY src/ ./src/

# Configure, compile, strip debug symbols. The cache mount keeps the fetched and
# compiled FetchContent dependencies (Crow, spdlog, SQLiteCpp, jwt-cpp) between
# builds without baking them into the image, so a source change only recompiles
# our own code.
RUN --mount=type=cache,target=/src/build/release/_deps \
    cmake --preset release \
    && cmake --build --preset release \
    && strip --strip-unneeded build/release/fitplan

# ---------------------------------------------------------------------------
# Stage 2 "runtime" — no compiler, only the binary and the libs it loads.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS runtime

# The shared libraries `ldd build/release/fitplan` reports, nothing more.
# libssl3t64 / libcurl4t64 are Ubuntu 24.04's renamed libssl3 / libcurl4
# (64-bit time_t transition). libcurl4t64 backs the outbound HTTPS the app makes
# to Google's JWKS and the Brevo email API; ca-certificates gives it a trust
# store (without it every TLS handshake fails). curl (the CLI) is only for the
# HEALTHCHECK below.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libssl3t64 libsodium23 libsqlite3-0 libcurl4t64 curl \
    && rm -rf /var/lib/apt/lists/*

# An unprivileged account to run the server as.
RUN useradd --system --no-create-home --shell /usr/sbin/nologin fitplan

WORKDIR /app

# The compiled server, then the static files it reads at run time.
COPY --from=build --chown=fitplan:fitplan /src/build/release/fitplan ./fitplan
COPY --chown=fitplan:fitplan web/ ./web/
COPY --chown=fitplan:fitplan docs/ ./docs/
COPY --chown=fitplan:fitplan src/db/migrations/ ./migrations/
COPY --chown=fitplan:fitplan scripts/seed.sql ./scripts/seed.sql

# Where the app looks for everything inside the container. web/, docs/ and
# migrations/ are resolved relative to WORKDIR (Crow's static handler strips a
# leading "/", so an absolute web path would not resolve). The DB lives on an
# absolute path that docker-compose backs with a named volume.
ENV FITPLAN_HOST=0.0.0.0 \
    FITPLAN_PORT=8080 \
    FITPLAN_DB_PATH=/data/fitplan.db \
    FITPLAN_MIGRATIONS_DIR=migrations \
    FITPLAN_WEB_DIR=web \
    FITPLAN_DOCS_DIR=docs

RUN mkdir -p /data && chown fitplan:fitplan /data
USER fitplan

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -fsS http://localhost:8080/api/health || exit 1

CMD ["./fitplan"]