# Archive

A system for archiving, preserving and organizing projects, files, versions and old material that is no longer active but should not be deleted.

## Stack

- **Frontend**: React 18 + TypeScript + Vite + Tailwind CSS
- **Backend**: Express + TypeScript + better-sqlite3
- **Database**: SQLite (WAL mode)
- **Storage**: Local filesystem

## Getting Started

```bash
npm run install:all
npm run dev
```

- Frontend: http://localhost:5173
- Backend: http://localhost:3001

## Features

- Import files, folders, and projects
- Automatic metadata extraction
- SHA-256 checksums for integrity
- Categories and tags
- Full-text search
- Version history
- Notes and activity log
- Favorites and trash

## Project Structure

```
Archive/
  client/          React frontend
  server/          Express backend
  shared/          Shared types (planned)
```

## Development

```bash
npm run dev          # Start both client and server
npm run build        # Build for production
npm run typecheck    # Type check both projects
npm test             # Run client tests
```
