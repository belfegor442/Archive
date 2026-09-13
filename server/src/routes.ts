import { Router } from "express";
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

function now() {
  return new Date().toISOString();
}

function logActivity(itemId: string, action: string, details: string = "") {
  db.prepare(
    "INSERT INTO activity (id, item_id, action, details, created_at) VALUES (?, ?, ?, ?, ?)"
  ).run(uuidv4(), itemId, action, details, now());
}

// List items
router.get("/items", (req, res) => {
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

  if (type) {
    query += " AND type = ?";
    params.push(type as string);
  }

  if (categoryId) {
    query += " AND category_id = ?";
    params.push(categoryId as string);
  }

  query += " ORDER BY archived_at DESC";

  const items = db.prepare(query).all(...params) as any[];

  const result = items.map((item) => ({
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
        `SELECT t.* FROM tags t
         JOIN item_tags it ON t.id = it.tag_id
         WHERE it.item_id = ?`
      )
      .all(item.id),
    category: item.category_id
      ? db.prepare("SELECT * FROM categories WHERE id = ?").get(item.category_id)
      : null,
  }));

  res.json(result);
});

// Get single item
router.get("/items/:id", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;

  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  const result = {
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
        `SELECT t.* FROM tags t
         JOIN item_tags it ON t.id = it.tag_id
         WHERE it.item_id = ?`
      )
      .all(item.id),
    category: item.category_id
      ? db.prepare("SELECT * FROM categories WHERE id = ?").get(item.category_id)
      : null,
  };

  res.json(result);
});

// Import items
router.post("/items/import", (req, res) => {
  const { paths, categoryId, tags, description } = req.body;

  if (!Array.isArray(paths) || paths.length === 0) {
    return res.status(400).json({ message: "paths array is required" });
  }

  const createdItems: any[] = [];

  const importTransaction = db.transaction(() => {
    for (const itemPath of paths) {
      if (!fs.existsSync(itemPath)) {
        continue;
      }

      const stat = fs.statSync(itemPath);
      const itemType = detectItemType(itemPath);
      const itemName = getFileName(itemPath);
      const itemId = uuidv4();
      const size = stat.isDirectory() ? getDirSize(itemPath) : stat.size;
      const fileCount = stat.isDirectory() ? countFiles(itemPath) : 1;

      const storagePath = copyToStorage(itemPath, itemId);
      const checksum = stat.isFile()
        ? computeChecksum(itemPath)
        : "";

      const nowStr = now();

      db.prepare(
        `INSERT INTO archive_items (
          id, name, type, status, description, original_path, storage_path,
          size, file_count, category_id, created_at, archived_at,
          last_modified_at, checksum, current_version, is_favorite
        ) VALUES (?, ?, ?, 'archived', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 0)`
      ).run(
        itemId,
        itemName,
        itemType,
        description || "",
        itemPath,
        storagePath,
        size,
        fileCount,
        categoryId || null,
        stat.birthtime.toISOString(),
        nowStr,
        nowStr,
        checksum
      );

      if (tags && Array.isArray(tags)) {
        for (const tagName of tags) {
          let tag = db.prepare("SELECT * FROM tags WHERE name = ?").get(tagName) as any;
          if (!tag) {
            const tagId = uuidv4();
            db.prepare("INSERT INTO tags (id, name) VALUES (?, ?)").run(tagId, tagName);
            tag = { id: tagId, name: tagName };
          }
          db.prepare("INSERT OR IGNORE INTO item_tags (item_id, tag_id) VALUES (?, ?)").run(
            itemId,
            tag.id
          );
        }
      }

      logActivity(itemId, "Imported", `Archived from ${itemPath}`);

      // Create initial version
      db.prepare(
        `INSERT INTO versions (id, item_id, version_number, storage_path, checksum, size, notes, created_at)
         VALUES (?, ?, 1, ?, ?, ?, 'Initial version', ?)`
      ).run(uuidv4(), itemId, storagePath, checksum, size, nowStr);

      const createdItem = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(itemId);
      createdItems.push({
        ...(createdItem as any),
        isFavorite: false,
        tags: tags
          ? db
              .prepare(
                `SELECT t.* FROM tags t
                 JOIN item_tags it ON t.id = it.tag_id
                 WHERE it.item_id = ?`
              )
              .all(itemId)
          : [],
      });
    }
  });

  importTransaction();
  res.json(createdItems);
});

// Update item
router.put("/items/:id", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;
  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  const updates: string[] = [];
  const params: any[] = [];

  if (req.body.name !== undefined) {
    updates.push("name = ?");
    params.push(req.body.name);
  }
  if (req.body.description !== undefined) {
    updates.push("description = ?");
    params.push(req.body.description);
  }
  if (req.body.categoryId !== undefined) {
    updates.push("category_id = ?");
    params.push(req.body.categoryId);
  }
  if (req.body.isFavorite !== undefined) {
    updates.push("is_favorite = ?");
    params.push(req.body.isFavorite ? 1 : 0);
  }
  if (req.body.status !== undefined) {
    updates.push("status = ?");
    params.push(req.body.status);
  }

  updates.push("last_modified_at = ?");
  params.push(now());

  params.push(req.params.id);

  db.prepare(`UPDATE archive_items SET ${updates.join(", ")} WHERE id = ?`).run(...params);

  logActivity(req.params.id, "Updated", "Item metadata updated");

  const updated = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;
  res.json({
    ...updated,
    isFavorite: Boolean(updated.is_favorite),
    categoryId: updated.category_id,
    originalPath: updated.original_path,
    storagePath: updated.storage_path,
    fileCount: updated.file_count,
    currentVersion: updated.current_version,
    createdAt: updated.created_at,
    archivedAt: updated.archived_at,
    lastModifiedAt: updated.last_modified_at,
    tags: db
      .prepare(
        `SELECT t.* FROM tags t
         JOIN item_tags it ON t.id = it.tag_id
         WHERE it.item_id = ?`
      )
      .all(updated.id),
  });
});

// Delete item (soft)
router.delete("/items/:id", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id);
  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  db.prepare("UPDATE archive_items SET status = 'deleted', last_modified_at = ? WHERE id = ?").run(
    now(),
    req.params.id
  );
  logActivity(req.params.id, "Deleted", "Moved to trash");
  res.json({ message: "Item deleted" });
});

// Restore item
router.post("/items/:id/restore", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;
  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  db.prepare(
    "UPDATE archive_items SET status = 'archived', last_modified_at = ? WHERE id = ?"
  ).run(now(), req.params.id);
  logActivity(req.params.id, "Restored", "Restored from trash");

  const updated = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;
  res.json({
    ...updated,
    isFavorite: Boolean(updated.is_favorite),
    categoryId: updated.category_id,
    originalPath: updated.original_path,
    storagePath: updated.storage_path,
    fileCount: updated.file_count,
    currentVersion: updated.current_version,
    createdAt: updated.created_at,
    archivedAt: updated.archived_at,
    lastModifiedAt: updated.last_modified_at,
    tags: db
      .prepare(
        `SELECT t.* FROM tags t
         JOIN item_tags it ON t.id = it.tag_id
         WHERE it.item_id = ?`
      )
      .all(updated.id),
  });
});

// Get versions
router.get("/items/:id/versions", (req, res) => {
  const versions = db
    .prepare("SELECT * FROM versions WHERE item_id = ? ORDER BY version_number DESC")
    .all(req.params.id);
  res.json(versions);
});

// Create version
router.post("/items/:id/versions", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id) as any;
  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  const newVersion = item.current_version + 1;
  const nowStr = now();

  db.prepare(
    `INSERT INTO versions (id, item_id, version_number, storage_path, checksum, size, notes, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(
    uuidv4(),
    req.params.id,
    newVersion,
    item.storage_path,
    item.checksum,
    item.size,
    req.body.notes || "",
    nowStr
  );

  db.prepare(
    "UPDATE archive_items SET current_version = ?, last_modified_at = ? WHERE id = ?"
  ).run(newVersion, nowStr, req.params.id);

  logActivity(req.params.id, "Version created", `Version ${newVersion}`);

  const version = db
    .prepare("SELECT * FROM versions WHERE item_id = ? AND version_number = ?")
    .get(req.params.id, newVersion);
  res.json(version);
});

// Get notes
router.get("/items/:id/notes", (req, res) => {
  const notes = db
    .prepare("SELECT * FROM notes WHERE item_id = ? ORDER BY created_at DESC")
    .all(req.params.id);
  res.json(notes);
});

// Add note
router.post("/items/:id/notes", (req, res) => {
  const item = db.prepare("SELECT * FROM archive_items WHERE id = ?").get(req.params.id);
  if (!item) {
    return res.status(404).json({ message: "Item not found" });
  }

  const noteId = uuidv4();
  const nowStr = now();

  db.prepare(
    "INSERT INTO notes (id, item_id, content, created_at, updated_at) VALUES (?, ?, ?, ?, ?)"
  ).run(noteId, req.params.id, req.body.content, nowStr, nowStr);

  logActivity(req.params.id, "Note added", req.body.content.substring(0, 100));

  const note = db.prepare("SELECT * FROM notes WHERE id = ?").get(noteId);
  res.json(note);
});

// Get activity
router.get("/items/:id/activity", (req, res) => {
  const activities = db
    .prepare("SELECT * FROM activity WHERE item_id = ? ORDER BY created_at DESC")
    .all(req.params.id);
  res.json(activities);
});

// Categories
router.get("/categories", (_req, res) => {
  const categories = db.prepare("SELECT * FROM categories ORDER BY name").all();
  res.json(categories);
});

router.post("/categories", (req, res) => {
  const id = uuidv4();
  const { name, description, color, icon, parentId } = req.body;

  db.prepare(
    "INSERT INTO categories (id, name, description, color, icon, parent_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
  ).run(id, name, description || "", color || "#6366f1", icon || "", parentId || null, now());

  const category = db.prepare("SELECT * FROM categories WHERE id = ?").get(id);
  res.json(category);
});

router.put("/categories/:id", (req, res) => {
  const existing = db.prepare("SELECT * FROM categories WHERE id = ?").get(req.params.id);
  if (!existing) {
    return res.status(404).json({ message: "Category not found" });
  }

  const { name, description, color, icon } = req.body;
  const updates: string[] = [];
  const params: any[] = [];

  if (name !== undefined) { updates.push("name = ?"); params.push(name); }
  if (description !== undefined) { updates.push("description = ?"); params.push(description); }
  if (color !== undefined) { updates.push("color = ?"); params.push(color); }
  if (icon !== undefined) { updates.push("icon = ?"); params.push(icon); }

  params.push(req.params.id);
  db.prepare(`UPDATE categories SET ${updates.join(", ")} WHERE id = ?`).run(...params);

  const category = db.prepare("SELECT * FROM categories WHERE id = ?").get(req.params.id);
  res.json(category);
});

router.delete("/categories/:id", (req, res) => {
  db.prepare("DELETE FROM categories WHERE id = ?").run(req.params.id);
  res.json({ message: "Category deleted" });
});

// Search
router.get("/search", (req, res) => {
  const { q, type } = req.query;
  if (!q) {
    return res.json({ items: [], total: 0, query: "" });
  }

  const searchTerm = `%${q}%`;
  let query = "SELECT * FROM archive_items WHERE status != 'deleted'";
  const params: string[] = [];

  query += " AND (name LIKE ? OR description LIKE ? OR original_path LIKE ?)";
  params.push(searchTerm, searchTerm, searchTerm);

  if (type) {
    query += " AND type = ?";
    params.push(type as string);
  }

  query += " ORDER BY archived_at DESC";

  const items = db.prepare(query).all(...params) as any[];

  const result = items.map((item) => ({
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
        `SELECT t.* FROM tags t
         JOIN item_tags it ON t.id = it.tag_id
         WHERE it.item_id = ?`
      )
      .all(item.id),
  }));

  res.json({ items: result, total: result.length, query: q });
});

// Stats
router.get("/stats", (_req, res) => {
  const totalItems = (
    db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status != 'deleted'").get() as any
  ).count;
  const archivedItems = (
    db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status = 'archived'").get() as any
  ).count;
  const favoriteItems = (
    db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE is_favorite = 1").get() as any
  ).count;
  const deletedItems = (
    db.prepare("SELECT COUNT(*) as count FROM archive_items WHERE status = 'deleted'").get() as any
  ).count;
  const totalSize = (
    db.prepare("SELECT COALESCE(SUM(size), 0) as total FROM archive_items WHERE status != 'deleted'").get() as any
  ).total;

  const recentItems = db
    .prepare(
      "SELECT * FROM archive_items WHERE status != 'deleted' ORDER BY archived_at DESC LIMIT 6"
    )
    .all()
    .map((item: any) => ({
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
    }));

  const categoryCounts = db
    .prepare(
      `SELECT c.id as categoryId, c.name, c.color, COUNT(ai.id) as count
       FROM categories c
       LEFT JOIN archive_items ai ON c.id = ai.category_id AND ai.status != 'deleted'
       GROUP BY c.id
       HAVING count > 0
       ORDER BY count DESC`
    )
    .all();

  res.json({
    totalItems,
    archivedItems,
    favoriteItems,
    deletedItems,
    totalSize,
    recentItems,
    categoryCounts,
  });
});

export default router;
