import { Router, Request, Response } from "express";
import { v4 as uuidv4 } from "uuid";
import db from "./db";
import {
  copyToStorage,
  computeChecksum,
  detectItemType,
  getFileName,
  getDirSize,
  countFiles,
} from "./storage";
import fs from "fs";

const router = Router();

function p(req: Request, name: string): string {
  const val = req.params[name];
  return Array.isArray(val) ? val[0] : val;
}

function now() {
  return new Date().toISOString();
}

function logActivity(itemId: string, action: string, details: string = "") {
  db.prepare(
    "INSERT INTO activity (id, item_id, action, details, created_at) VALUES (?, ?, ?, ?, ?)"
  ).run(uuidv4(), itemId, action, details, now());
}

function transformItem(item: any) {
  if (!item) return null;
  return {
    ...item,
    isFavorite: Boolean(item.is_favorite),
    categoryId: item.category_id,
    originalPath: item.original_path,
    storagePath: item.storage_path,
    fileCount: item.file_count,
    currentVersion: item.current_version,
    createdAt: item.created_at,
    archivedAt: item.archived_at,
    lastModifiedAt: item.last_modified_at,
    tags: db
      .prepare(
        `SELECT t.* FROM tags t JOIN item_tags it ON t.id = it.tag_id WHERE it.item_id = ?`
      )
      .all(item.id),
    category: item.category_id
      ? db.prepare("SELECT * FROM categories WHERE id = ?").get(item.category_id)
      : null,
  };
}

// List items
router.get("/items", (req: Request, res: Response) => {
  const { status, type, categoryId } = req.query;
  let query = "SELECT * FROM archive_items WHERE 1=1";
  const params: string[] = [];

  if (status) {
    if (status === "favorite") {
      query += " AND is_favorite = 1 AND status != 'deleted'";
    } else {
      query += " AND status = ?";
      params.push(status as string);
    }
  }
  if (type) { query += " AND type = ?"; params.push(type as string); }
  if (categoryId) { query += " AND category_id = ?"; params.push(categoryId as string); }

  query += " ORDER BY archived_at DESC";
  const items = db.prepare(query).all(...params) as any[];
  res.json(items.map(transformItem));
});

// Get single item
router.get("/items/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }
  res.json(transformItem(item));
});

// Import items
router.post("/items/import", (req: Request, res: Response) => {
  const { paths, categoryId, tags, description } = req.body;
  if (!Array.isArray(paths) || paths.length === 0) {
    res.status(400).json({ message: "paths array is required" }); return;
  }

  const createdItems: any[] = [];
  const errors: { path: string; error: string }[] = [];

  const importTransaction = db.transaction(() => {
    for (const itemPath of paths) {
      try {
        if (!fs.existsSync(itemPath)) {
          errors.push({ path: itemPath, error: "Path does not exist" }); continue;
        }
        const stat = fs.statSync(itemPath);
        const itemType = detectItemType(itemPath);
        const itemName = getFileName(itemPath);
        const itemId = uuidv4();
        const size = stat.isDirectory() ? getDirSize(itemPath) : stat.size;
        const fileCount = stat.isDirectory() ? countFiles(itemPath) : 1;
        const storagePath = copyToStorage(itemPath, itemId);
        const checksum = stat.isFile() ? computeChecksum(itemPath) : "";
        const nowStr = now();

        db.prepare(
          `INSERT INTO archive_items (id, name, type, status, description, original_path, storage_path,
           size, file_count, category_id, created_at, archived_at, last_modified_at, checksum, current_version, is_favorite)
           VALUES (?, ?, ?, 'archived', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 0)`
        ).run(itemId, itemName, itemType, description || "", itemPath, storagePath, size, fileCount,
          categoryId || null, stat.birthtime.toISOString(), nowStr, nowStr, checksum);

        if (tags && Array.isArray(tags)) {
          for (const tagName of tags) {
            let tag = db.prepare("SELECT * FROM tags WHERE name = ?").get(tagName) as any;
            if (!tag) {
              const tagId = uuidv4();
              db.prepare("INSERT INTO tags (id, name) VALUES (?, ?)").run(tagId, tagName);
              tag = { id: tagId, name: tagName };
            }
            db.prepare("INSERT OR IGNORE INTO item_tags (item_id, tag_id) VALUES (?, ?)").run(itemId, tag.id);
          }
        }

        logActivity(itemId, "Imported", `Archived from ${itemPath}`);
        db.prepare(
          `INSERT INTO versions (id, item_id, version_number, storage_path, checksum, size, notes, created_at)
           VALUES (?, ?, 1, ?, ?, ?, 'Initial version', ?)`
        ).run(uuidv4(), itemId, storagePath, checksum, size, nowStr);

        const createdItem = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(itemId);
        createdItems.push(transformItem(createdItem));
      } catch (err) {
        errors.push({ path: itemPath, error: err instanceof Error ? err.message : "Unknown error" });
      }
    }
  });

  importTransaction();
  res.json({ items: createdItems, errors });
});

// Update item
router.put("/items/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }

  const updates: string[] = [];
  const params: any[] = [];
  if (req.body.name !== undefined) { updates.push("name = ?"); params.push(req.body.name); }
  if (req.body.description !== undefined) { updates.push("description = ?"); params.push(req.body.description); }
  if (req.body.categoryId !== undefined) { updates.push("category_id = ?"); params.push(req.body.categoryId); }
  if (req.body.isFavorite !== undefined) { updates.push("is_favorite = ?"); params.push(req.body.isFavorite ? 1 : 0); }
  if (req.body.status !== undefined) { updates.push("status = ?"); params.push(req.body.status); }

  if (updates.length === 0) { res.status(400).json({ message: "No fields to update" }); return; }

  updates.push("last_modified_at = ?");
  params.push(now());
  params.push(id);
  db.prepare(`UPDATE archive_items SET ${updates.join(", ")} WHERE id = ?`).run(...params);
  logActivity(id, "Updated", "Item metadata updated");

  const updated = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  res.json(transformItem(updated));
});

// Delete item (soft)
router.delete("/items/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id);
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }

  db.prepare("UPDATE archive_items SET status = 'deleted', last_modified_at = ? WHERE id = ?").run(now(), id);
  logActivity(id, "Deleted", "Moved to trash");
  res.json({ message: "Item deleted" });
});

// Restore item
router.post("/items/:id/restore", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }

  db.prepare("UPDATE archive_items SET status = 'archived', last_modified_at = ? WHERE id = ?").run(now(), id);
  logActivity(id, "Restored", "Restored from trash");

  const updated = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  res.json(transformItem(updated));
});

// Get versions
router.get("/items/:id/versions", (req: Request, res: Response) => {
  const id = p(req, "id");
  const versions = db.prepare("SELECT * FROM versions WHERE item_id = ? ORDER BY version_number DESC").all(id);
  res.json(versions);
});

// Create version
router.post("/items/:id/versions", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id) as any;
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }

  const newVersion = item.current_version + 1;
  const nowStr = now();

  db.prepare(
    `INSERT INTO versions (id, item_id, version_number, storage_path, checksum, size, notes, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(uuidv4(), id, newVersion, item.storage_path, item.checksum, item.size, req.body.notes || "", nowStr);

  db.prepare("UPDATE archive_items SET current_version = ?, last_modified_at = ? WHERE id = ?").run(newVersion, nowStr, id);
  logActivity(id, "Version created", `Version ${newVersion}`);

  const version = db.prepare("SELECT * FROM versions WHERE item_id = ? AND version_number = ?").get(id, newVersion);
  res.json(version);
});

// Get notes
router.get("/items/:id/notes", (req: Request, res: Response) => {
  const id = p(req, "id");
  const notes = db.prepare("SELECT * FROM notes WHERE item_id = ? ORDER BY created_at DESC").all(id);
  res.json(notes);
});

// Add note
router.post("/items/:id/notes", (req: Request, res: Response) => {
  const id = p(req, "id");
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(id);
  if (!item) { res.status(404).json({ message: "Item not found" }); return; }
  if (!req.body.content || typeof req.body.content !== "string") {
    res.status(400).json({ message: "content is required" }); return;
  }

  const noteId = uuidv4();
  const nowStr = now();
  db.prepare("INSERT INTO notes (id, item_id, content, created_at, updated_at) VALUES (?, ?, ?, ?, ?)")
    .run(noteId, id, req.body.content, nowStr, nowStr);
  logActivity(id, "Note added", req.body.content.substring(0, 100));

  const note = db.prepare("SELECT * FROM notes WHERE id = ?").get(noteId);
  res.json(note);
});

// Delete note
router.delete("/items/:itemId/notes/:noteId", (req: Request, res: Response) => {
  const noteId = p(req, "noteId");
  const itemId = p(req, "itemId");
  const note = db.prepare("SELECT * FROM notes WHERE id = ? AND item_id = ?").get(noteId, itemId);
  if (!note) { res.status(404).json({ message: "Note not found" }); return; }

  db.prepare("DELETE FROM notes WHERE id = ?").run(noteId);
  res.json({ message: "Note deleted" });
});

// Get activity
router.get("/items/:id/activity", (req: Request, res: Response) => {
  const id = p(req, "id");
  const activities = db.prepare("SELECT * FROM activity WHERE item_id = ? ORDER BY created_at DESC").all(id);
  res.json(activities);
});

// Categories
router.get("/categories", (_req: Request, res: Response) => {
  const categories = db.prepare("SELECT * FROM categories ORDER BY name").all();
  res.json(categories);
});

router.post("/categories", (req: Request, res: Response) => {
  const { name, description, color, icon, parentId } = req.body;
  if (!name || typeof name !== "string") { res.status(400).json({ message: "name is required" }); return; }

  const id = uuidv4();
  db.prepare(
    "INSERT INTO categories (id, name, description, color, icon, parent_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
  ).run(id, name, description || "", color || "#6366f1", icon || "", parentId || null, now());

  const category = db.prepare("SELECT * FROM categories WHERE id = ?").get(id);
  res.json(category);
});

router.put("/categories/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  const existing = db.prepare("SELECT * FROM categories WHERE id = ?").get(id);
  if (!existing) { res.status(404).json({ message: "Category not found" }); return; }

  const { name, description, color, icon } = req.body;
  const updates: string[] = [];
  const params: any[] = [];
  if (name !== undefined) { updates.push("name = ?"); params.push(name); }
  if (description !== undefined) { updates.push("description = ?"); params.push(description); }
  if (color !== undefined) { updates.push("color = ?"); params.push(color); }
  if (icon !== undefined) { updates.push("icon = ?"); params.push(icon); }

  if (updates.length === 0) { res.status(400).json({ message: "No fields to update" }); return; }

  params.push(id);
  db.prepare(`UPDATE categories SET ${updates.join(", ")} WHERE id = ?`).run(...params);
  const category = db.prepare("SELECT * FROM categories WHERE id = ?").get(id);
  res.json(category);
});

router.delete("/categories/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  const existing = db.prepare("SELECT * FROM categories WHERE id = ?").get(id);
  if (!existing) { res.status(404).json({ message: "Category not found" }); return; }

  db.prepare("UPDATE archive_items SET category_id = NULL WHERE category_id = ?").run(id);
  db.prepare("DELETE FROM categories WHERE id = ?").run(id);
  res.json({ message: "Category deleted" });
});

// Tags
router.get("/tags", (_req: Request, res: Response) => {
  const tags = db.prepare("SELECT * FROM tags ORDER BY name").all();
  res.json(tags);
});

router.post("/tags", (req: Request, res: Response) => {
  const { name, color } = req.body;
  if (!name || typeof name !== "string") { res.status(400).json({ message: "name is required" }); return; }

  const existing = db.prepare("SELECT * FROM tags WHERE name = ?").get(name);
  if (existing) { res.json(existing); return; }

  const id = uuidv4();
  db.prepare("INSERT INTO tags (id, name, color) VALUES (?, ?, ?)").run(id, name, color || "#6366f1");
  const tag = db.prepare("SELECT * FROM tags WHERE id = ?").get(id);
  res.json(tag);
});

router.delete("/tags/:id", (req: Request, res: Response) => {
  const id = p(req, "id");
  db.prepare("DELETE FROM tags WHERE id = ?").run(id);
  res.json({ message: "Tag deleted" });
});

// Search (FTS5 with LIKE fallback)
router.get("/search", (req: Request, res: Response) => {
  const { q, type } = req.query;
  if (!q || typeof q !== "string") { res.json({ items: [], total: 0, query: "" }); return; }

  try {
    const searchTerm = q.replace(/[^\w\s]/g, " ").trim();
    if (!searchTerm) { res.json({ items: [], total: 0, query: q }); return; }

    let ftsQuery = `SELECT ai.* FROM archive_items ai JOIN items_fts fts ON ai.rowid = fts.rowid
      WHERE items_fts MATCH ? AND ai.status != 'deleted'`;
    const params: string[] = [searchTerm + "*"];
    if (type) { ftsQuery += " AND ai.type = ?"; params.push(type as string); }
    ftsQuery += " ORDER BY ai.archived_at DESC LIMIT 50";

    const items = db.prepare(ftsQuery).all(...params) as any[];
    res.json({ items: items.map(transformItem), total: items.length, query: q });
  } catch {
    const searchTerm = `%${q}%`;
    let likeQuery = "SELECT * FROM archive_items WHERE status != 'deleted'";
    const params: string[] = [];
    likeQuery += " AND (name LIKE ? OR description LIKE ? OR original_path LIKE ?)";
    params.push(searchTerm, searchTerm, searchTerm);
    if (type) { likeQuery += " AND type = ?"; params.push(type as string); }
    likeQuery += " ORDER BY archived_at DESC";
    const items = db.prepare(likeQuery).all(...params) as any[];
    res.json({ items: items.map(transformItem), total: items.length, query: q });
  }
});

// Stats
router.get("/stats", (_req: Request, res: Response) => {
  const totalItems = (db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status != 'deleted'").get() as any).count;
  const archivedItems = (db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status = 'archived'").get() as any).count;
  const favoriteItems = (db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE is_favorite = 1").get() as any).count;
  const deletedItems = (db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status = 'deleted'").get() as any).count;
  const totalSize = (db.prepare("SELECT COALESCE(SUM(size), 0) as total FROM archive_items WHERE status != 'deleted'").get() as any).total;

  const recentItems = db.prepare("SELECT * FROM archive_items WHERE status != 'deleted' ORDER BY archived_at DESC LIMIT 6").all().map(transformItem);
  const categoryCounts = db.prepare(`SELECT c.id as categoryId, c.name, c.color, COUNT(ai.id) as count FROM categories c LEFT JOIN archive_items ai ON c.id = ai.category_id AND ai.status != 'deleted' GROUP BY c.id HAVING count > 0 ORDER BY count DESC`).all();
  const typeCounts = db.prepare(`SELECT type, COUNT(*) as count FROM archive_items WHERE status != 'deleted' GROUP BY type ORDER BY count DESC`).all();

  res.json({ totalItems, archivedItems, favoriteItems, deletedItems, totalSize, recentItems, categoryCounts, typeCounts });
});

// Global error handler
router.use((err: Error, _req: Request, res: Response, _next: Function) => {
  console.error("Route error:", err);
  res.status(500).json({ message: "Internal server error" });
});

export default router;
