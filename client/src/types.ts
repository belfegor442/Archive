export type ItemType = "project" | "file" | "folder" | "document";
export type ItemStatus = "archived" | "deleted" | "favorite";

export interface Category {
  id: string;
  name: string;
  description: string;
  color: string;
  icon: string;
  parentId: string | null;
  createdAt: string;
}

export interface Tag {
  id: string;
  name: string;
  color: string;
}

export interface ArchiveItem {
  id: string;
  name: string;
  type: ItemType;
  status: ItemStatus;
  description: string;
  originalPath: string;
  storagePath: string;
  size: number;
  fileCount: number;
  categoryId: string | null;
  category?: Category;
  tags?: Tag[];
  createdAt: string;
  archivedAt: string;
  lastModifiedAt: string;
  checksum: string;
  currentVersion: number;
  isFavorite: boolean;
}

export interface Version {
  id: string;
  itemId: string;
  versionNumber: number;
  storagePath: string;
  checksum: string;
  size: number;
  notes: string;
  createdAt: string;
}

export interface Note {
  id: string;
  itemId: string;
  content: string;
  createdAt: string;
  updatedAt: string;
}

export interface Activity {
  id: string;
  itemId: string;
  action: string;
  details: string;
  createdAt: string;
}

export interface DashboardStats {
  totalItems: number;
  archivedItems: number;
  favoriteItems: number;
  deletedItems: number;
  totalSize: number;
  recentItems: ArchiveItem[];
  categoryCounts: { categoryId: string; name: string; count: number; color: string }[];
}

export interface ImportRequest {
  paths: string[];
  categoryId?: string;
  tags?: string[];
  description?: string;
}

export interface SearchResult {
  items: ArchiveItem[];
  total: number;
  query: string;
}
