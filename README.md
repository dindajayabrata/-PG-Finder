# PG Finder: C++ API backend with Oracle PL/SQL

This project connects the supplied HTML, CSS, and JavaScript frontend to the Oracle tables and PL/SQL procedures. The C++ server serves the website and exposes the JSON API already called by `public/js`.

## What is included

- The supplied frontend, arranged as `public/pages`, `public/css`, and `public/js`.
- A C++17 HTTP server on `http://localhost:3000`.
- Oracle database access through ODBC.
- Calls to the existing `sp_register_user` and `sp_toggle_favorite` procedures.
- SQL queries/inserts for login, room listings, favorites, new rooms, and contact messages.
- The supplied SQL setup scripts in `database/`.

## API routes used by the frontend

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/api/rooms` | Load room listings |
| POST | `/api/register` | Register a user using `sp_register_user` |
| POST | `/api/login` | Check email and password |
| GET | `/api/favorites/{userId}` | Load a user's saved room IDs |
| POST | `/api/favorites/toggle` | Add/remove a favorite using `sp_toggle_favorite` |
| POST | `/api/rooms` | Save a room listing |
| POST | `/api/contact` | Save a contact message |

The JavaScript API URL is set to `/api`, so the pages and backend use the same local server.

## Before running

1. Install Oracle Database and create/use the same schema in which your PG Finder tables exist.
2. If the tables do not exist yet, run `database/schema_and_procedures.sql` once in SQL Developer using **F5 (Run Script)**. This script creates the tables, sequences, triggers, procedures, and sample records. It is marked fresh-setup-only; do not rerun it on existing tables.
3. If you restored or manually inserted records with IDs, run `database/adjust_existing_sequences.sql` once to advance the sequences. It is not needed for a normal fresh setup.
4. Install a 64-bit Oracle ODBC driver that matches the C++ build. In ODBC Data Sources, note the exact driver name. If the Oracle ODBC driver is missing, install the Oracle Instant Client ODBC package that matches your Oracle client and Windows architecture.
5. Install Visual Studio C++ Build Tools, CMake, and vcpkg. From this project folder, run:

```bat
vcpkg install
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Replace `C:/vcpkg` with the folder where you installed vcpkg. The first build downloads the two C++ header libraries listed in `vcpkg.json`.

## Configure the Oracle connection

Open Command Prompt in the project folder. Set the connection string for the Oracle ODBC driver installed on your computer. Example for a common Oracle XE 21c setup:

```bat
set "PGFINDER_ORACLE_CONNECTION=Driver={Oracle in XE};Dbq=localhost:1521/XEPDB1;Uid=YOUR_SCHEMA;Pwd=YOUR_PASSWORD;"
```

Replace `Oracle in XE` with the exact ODBC driver name shown on your computer. Replace `YOUR_SCHEMA` and `YOUR_PASSWORD` with the database user and password that own or can access the PG Finder tables. If your database uses a different service name, replace `XEPDB1` with that service name.

Then start the server:

```bat
build\Release\pg_finder_backend.exe
```

Open **http://localhost:3000** in your browser. The website files must stay inside the `public` folder, and the server must be started from the project folder so it can find that folder.

The sample database script creates a demo account:

- Email: `user@example.com`
- Password: `password123`

## How the connection works

```text
Browser pages and JavaScript -> C++ API server -> Oracle ODBC -> Oracle tables / PL/SQL procedures
```

The API uses parameterized SQL for form values. Room city is taken from the text after the last comma in the existing location field, so the frontend form does not need another input.

## Current project scope

This is a local academic demo. The supplied registration procedure saves the password value directly in `password_hash`, and the current frontend keeps the logged-in user in `localStorage`. Use demo credentials only. For real users, password hashing and server-side login sessions should be added before deployment.
