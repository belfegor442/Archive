import crypto from "crypto";
import fs from "fs";
import path from "path";

const ITEMS_DIR = path.join(process.env.HOME || process.env.USERPROFILE || ".", "archive-data", "items");

fs.mkdirSync(ITEMS_DIR, { recursive: true });

export function getItemStorageDir(itemId: string): string {
  const dir = path.join(ITEMS_DIR, itemId);
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

export function getVersionDir(itemId: string, version: number): string {
  const dir = path.join(getItemStorageDir(itemId), "versions", `v${version}`);
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

export function computeChecksum(filePath: string): string {
  const data = fs.readFileSync(filePath);
  return crypto.createHash("sha256").update(data).digest("hex");
}

export function copyToStorage(sourcePath: string, itemId: string): string {
  const destDir = getItemStorageDir(itemId);
  const destPath = path.join(destDir, "files");

  if (fs.statSync(sourcePath).isDirectory()) {
    copyDirectorySync(sourcePath, destPath);
  } else {
    fs.mkdirSync(path.dirname(destPath), { recursive: true });
    fs.copyFileSync(sourcePath, destPath);
  }

  return destPath;
}

function copyDirectorySync(src: string, dest: string): void {
  fs.mkdirSync(dest, { recursive: true });
  const entries = fs.readdirSync(src, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);

    if (entry.isDirectory()) {
      copyDirectorySync(srcPath, destPath);
    } else {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

export function getDirSize(dirPath: string): number {
  let size = 0;
  const entries = fs.readdirSync(dirPath, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);
    if (entry.isDirectory()) {
      size += getDirSize(fullPath);
    } else {
      size += fs.statSync(fullPath).size;
    }
  }

  return size;
}

export function countFiles(dirPath: string): number {
  let count = 0;
  const entries = fs.readdirSync(dirPath, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);
    if (entry.isDirectory()) {
      count += countFiles(fullPath);
    } else {
      count++;
    }
  }

  return count;
}

export function detectItemType(itemPath: string): "project" | "file" | "folder" | "document" {
  const stat = fs.statSync(itemPath);

  if (stat.isFile()) {
    const ext = path.extname(itemPath).toLowerCase();
    const docExts = [
      ".md", ".txt", ".pdf", ".doc", ".docx", ".xls", ".xlsx",
      ".ppt", ".pptx", ".csv", ".rtf", ".odt", ".ods", ".odp",
    ];
    return docExts.includes(ext) ? "document" : "file";
  }

  if (stat.isDirectory()) {
    const entries = fs.readdirSync(itemPath);
    const indicators = ["package.json", "Cargo.toml", "go.mod", "pom.xml", "build.gradle", "requirements.txt", "setup.py", "pyproject.toml", ".git"];
    const isProject = indicators.some((i) => entries.includes(i));
    return isProject ? "project" : "folder";
  }

  return "file";
}

export function getFileName(itemPath: string): string {
  return path.basename(itemPath);
}
