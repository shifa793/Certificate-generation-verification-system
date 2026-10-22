# Certificate System (C++ + MySQL)

A small certificate issuing and verification project. The browser frontend calls a C++ HTTP API; the C++ backend stores and checks records in MySQL using the MariaDB/MySQL C client library. Issued certificates can be printed or saved as PDF through the browser print dialog.

## Project structure

- `frontend/` — HTML, CSS and JavaScript UI
- `backend/main.cpp` — C++ HTTP server and MySQL queries
- `backend/build.bat` — Windows build command for MSYS2 UCRT64
- `database/schema.sql` — database and table schema

## Requirements (Windows)

1. MySQL or MariaDB running locally (XAMPP MySQL is fine).
2. MSYS2 UCRT64 GCC and MariaDB client development library. Use the UCRT64 compiler and libraries together; **do not mix `C:\MinGW\bin\g++.exe` with MSYS2 libraries**.
3. A modern browser.

If using MSYS2 UCRT64, open the **MSYS2 UCRT64** terminal and install the compiler/client development packages if missing:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libmariadbclient
```

The provided `build.bat` assumes MSYS2 is installed at `C:\msys64` and uses the UCRT64 MariaDB headers under `include\mariadb`.

## 1. Create the database

Start MySQL in XAMPP. Open phpMyAdmin, select the SQL tab, and run the contents of `database/schema.sql`. This creates `certificate_system` and the `certificates` table if they do not exist; it does not drop existing data.

If you already have a `certificate_system.certificates` table with these columns, keep it and do not re-create or delete it:

- `certificate_id VARCHAR(50)` (primary key)
- `student_name VARCHAR(100)`
- `course_name VARCHAR(150)`
- `issue_date DATE`
- `authorized_name VARCHAR(100)`

## 2. Configure database credentials

Open `backend/main.cpp` and update `DB_HOST`, `DB_USER`, `DB_PASS`, `DB_NAME` if your MySQL setup differs. The defaults are host `127.0.0.1`, user `root`, empty password, database `certificate_system`, port `3306`.

## 3. Build the C++ backend

The backend source includes explicit MariaDB Connector/C options to avoid requiring TLS for local XAMPP development. After changing `main.cpp`, you must rebuild; running an old `certificate_backend.exe` will still show the previous error.

From File Explorer, double-click `backend/build.bat`, or open PowerShell in `backend` and run:

```powershell
.\build.bat
```

The build uses the MSYS2 UCRT64 compiler and the matching MariaDB library. If the build fails, keep the terminal open and copy the full error. Do not use the separate MinGW compiler with these MSYS2 libraries. Do not use VS Code “Run Code” for this file; use `build.bat` so the correct headers and libraries are used.

## 4. Start the backend

Run `backend/certificate_backend.exe`. A console should say:

`Certificate System C++ API running at http://localhost:8080`

Keep this terminal open. If port 8080 is already in use, close the other service or change `PORT` in `main.cpp`, then rebuild.

## 5. Open the frontend

Open `frontend/index.html` in Chrome or Edge. If your browser blocks local-file fetch requests, use VS Code Live Server to serve the `frontend` folder, then open its localhost URL. The backend allows local frontend requests through CORS.

## 6. Test

- API health: open `http://localhost:8080/api/health`; expect a JSON success message.
- Verify an existing ID from phpMyAdmin, for example `CERT-1001` only if that exact ID exists in your table.
- Issue a test certificate, then copy its new ID and verify it.
- Use **Print / Save as PDF** to print the certificate.

## API routes

- `GET /api/health` — backend health check
- `GET /api/verify?id=CERT-...` — looks up an ID in MySQL
- `POST /api/generate` — URL-encoded fields `student_name`, `course_name`, `authorized_name`; inserts a certificate and returns its ID

## Notes / limitations

- This is a local academic/demo project, not a production certificate authority. Before public deployment, add authentication/authorization for issuance, rate limiting, HTTPS, stronger request parsing, audit logs, and restricted database permissions. The API currently binds to all network interfaces on port 8080; use only on a trusted local network.
- A database connection failure is logged to the backend console. Check MySQL is running, credentials are correct, and the database/table exists.
- The C++ backend is a minimal HTTP server intended for a local demo, not a hardened public web server.
