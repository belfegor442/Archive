import type {
  ArchiveItem,
  Category,
  DashboardStats,
  SearchResult,
  Version,
  Note,
  Activity,
  ImportRequest,
  Tag,
} from "./types";

const API_BASE = "/api";

async function request<T>(url: string, options?: RequestInit): Promise<T> {
  const res = await fetch(`${API_BASE}${url}`, {
    headers: {
      "Content-Type": "application/json",
      ...options?.headers,
    },
    ...options,
  });
  if (!res.ok) {
    const error = await res.json().catch(() => ({ message: res.statusText }));
    throw new Error(error.message || "Request failed");
  }
  return res.json();
}

export interface ImportResult {
  items: ArchiveItem[];
  errors: { path: string; error: string }[];
}

export const api = {
  items: {
    list: (params?: { status?: string; type?: string; categoryId?: string }) => {
      const query = new URLSearchParams();
      if (params?.status) query.set("status", params.status);
      if (params?.type) query.set("type", params.type);
      if (params?.categoryId) query.set("categoryId", params.categoryId);
      const qs = query.toString();
      return request<ArchiveItem[]>(`/items${qs ? `?${qs}` : ""}`);
    },
    get: (id: string) => request<ArchiveItem>(`/items/${id}`),
    create: (data: ImportRequest) =>
      request<ImportResult>("/items/import", {
        method: "POST",
        body: JSON.stringify(data),
      }),
    update: (id: string, data: Partial<ArchiveItem>) =>
      request<ArchiveItem>(`/items/${id}`, {
        method: "PUT",
        body: JSON.stringify(data),
      }),
    delete: (id: string) =>
      request<{ message: string }>(`/items/${id}`, { method: "DELETE" }),
    restore: (id: string) =>
      request<ArchiveItem>(`/items/${id}/restore`, { method: "POST" }),
    versions: (id: string) => request<Version[]>(`/items/${id}/versions`),
    addVersion: (id: string, notes: string) =>
      request<Version>(`/items/${id}/versions`, {
        method: "POST",
        body: JSON.stringify({ notes }),
      }),
    notes: (id: string) => request<Note[]>(`/items/${id}/notes`),
    addNote: (id: string, content: string) =>
      request<Note>(`/items/${id}/notes`, {
        method: "POST",
        body: JSON.stringify({ content }),
      }),
    deleteNote: (itemId: string, noteId: string) =>
      request<{ message: string }>(`/items/${itemId}/notes/${noteId}`, {
        method: "DELETE",
      }),
    activity: (id: string) => request<Activity[]>(`/items/${id}/activity`),
  },
  categories: {
    list: () => request<Category[]>("/categories"),
    create: (data: Partial<Category>) =>
      request<Category>("/categories", {
        method: "POST",
        body: JSON.stringify(data),
      }),
    update: (id: string, data: Partial<Category>) =>
      request<Category>(`/categories/${id}`, {
        method: "PUT",
        body: JSON.stringify(data),
      }),
    delete: (id: string) =>
      request<{ message: string }>(`/categories/${id}`, { method: "DELETE" }),
  },
  tags: {
    list: () => request<Tag[]>("/tags"),
    create: (data: { name: string; color?: string }) =>
      request<Tag>("/tags", {
        method: "POST",
        body: JSON.stringify(data),
      }),
    delete: (id: string) =>
      request<{ message: string }>(`/tags/${id}`, { method: "DELETE" }),
  },
  search: (query: string, params?: Record<string, string>) => {
    const searchParams = new URLSearchParams({ q: query });
    if (params) {
      Object.entries(params).forEach(([k, v]) => {
        if (v) searchParams.set(k, v);
      });
    }
    return request<SearchResult>(`/search?${searchParams.toString()}`);
  },
  stats: () => request<DashboardStats>("/stats"),
};
