# PointerThere Backend

PointerThere C++20 backend for PointerThere built with the Drogon Web Framework.

Licensed under the GNU General Public License v3.0 (GPLv3).

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 13+, or MSVC 2022)
- CMake 3.20+
- Drogon Framework
- PostgreSQL (Neon PostgreSQL)
- OpenSSL & libcurl

## Database Initialization

Initialize database tables using the schema file:

```bash
psql "connection_string" -f schema.sql
```

## Build Instructions

```bash
cp .env.example .env
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./pointerthere_backend
```

## Environment Configuration

See `.env.example` for required configuration variables:
- `PORT` & `HOST`
- `DATABASE_URL`
- `JWT_SECRET`
- `ADMIN_USERNAME` & `ADMIN_PASSWORD` & `ADMIN_SESSION_SECRET`
- `TURNSTILE_SECRET_KEY`
- `RESEND_API_KEY` & `FROM_EMAIL`
- `ALLOWED_ORIGIN`
