import Database from "better-sqlite3";
import path from "path";
import fs from "fs";

const DATA_DIR = path.join(process.env.HOME || process.env.USERPROFILE || ".", "archive-data");
const DB_PATH = path.join(DATA_DIR, "archive.db");

fs.mkdirSync(DATA_DIR, { recursive: true });

const db: InstanceType<typeof Database> = new Database(DB_PATH);

db.pragma("journal_mode = WAL");
db.pragma("foreign_keys = ON");

db.exec(`
  CREATE TABLE IF NOT EXISTS categories (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT DEFAULT '',
    color TEXT DEFAULT '#6366f1',
    icon TEXT DEFAULT '',
    parent_id TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (parent_id) REFERENCES categories(id) ON DELETE SET NULL
  );

  CREATE TABLE IF NOT EXISTS archive_items (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('project', 'file', 'folder', 'document')),
    status TEXT NOT NULL DEFAULT 'archived' CHECK(status IN ('archived', 'deleted', 'favorite')),
    description TEXT DEFAULT '',
    original_path TEXT NOT NULL,
    storage_path TEXT NOT NULL,
    size INTEGER DEFAULT 0,
    file_count INTEGER DEFAULT 0,
    category_id TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    archived_at TEXT NOT NULL DEFAULT (datetime('now')),
    last_modified_at TEXT NOT NULL DEFAULT (datetime('now')),
    checksum TEXT DEFAULT '',
    current_version INTEGER DEFAULT 1,
    is_favorite INTEGER DEFAULT 0,
    FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE SET NULL
  );

  CREATE TABLE IF NOT EXISTS tags (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    color TEXT DEFAULT '#6366f1'
  );

  CREATE TABLE IF NOT EXISTS item_tags (
    item_id TEXT NOT NULL,
    tag_id TEXT NOT NULL,
    PRIMARY KEY (item_id, tag_id),
    FOREIGN KEY (item_id) REFERENCES archive_items(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS versions (
    id TEXT PRIMARY KEY,
    item_id TEXT NOT NULL,
    version_number INTEGER NOT NULL,
    storage_path TEXT NOT NULL,
    checksum TEXT DEFAULT '',
    size INTEGER DEFAULT 0,
    notes TEXT DEFAULT '',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (item_id) REFERENCES archive_items(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS notes (
    id TEXT PRIMARY KEY,
    item_id TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (item_id) REFERENCES archive_items(id) ON DELETE CASCADE
  );

  CREATE TABLE IF NOT EXISTS activity (
    id TEXT PRIMARY KEY,
    item_id TEXT NOT NULL,
    action TEXT NOT NULL,
    details TEXT DEFAULT '',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (item_id) REFERENCES archive_items(id) ON DELETE CASCADE
  );

  CREATE INDEX IF NOT EXISTS idx_items_status ON archive_items(status);
  CREATE INDEX IF NOT EXISTS idx_items_type ON archive_items(type);
  CREATE INDEX IF NOT EXISTS idx_items_category ON archive_items(category_id);
  CREATE INDEX IF NOT EXISTS idx_items_archived ON archive_items(archived_at);
  CREATE INDEX IF NOT EXISTS idx_versions_item ON versions(item_id);
  CREATE INDEX IF NOT EXISTS idx_notes_item ON notes(item_id);
  CREATE INDEX IF NOT EXISTS idx_activity_item ON activity(item_id);
`);

export default db;
