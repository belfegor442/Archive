import { useEffect, useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem } from "../types";

export default function ItemsList() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [typeFilter, setTypeFilter] = useState("");
  const [sortOrder, setSortOrder] = useState<"newest" | "oldest" | "name">("newest");

  useEffect(() => {
    setLoading(true);
    api.items
      .list({ status: "archived", type: typeFilter || undefined })
      .then((data) => {
        const sorted = [...data].sort((a, b) => {
          if (sortOrder === "newest") return new Date(b.archivedAt).getTime() - new Date(a.archivedAt).getTime();
          if (sortOrder === "oldest") return new Date(a.archivedAt).getTime() - new Date(b.archivedAt).getTime();
          return a.name.localeCompare(b.name);
        });
        setItems(sorted);
      })
      .catch(console.error)
      .finally(() => setLoading(false));
  }, [typeFilter, sortOrder]);

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-archive-100">All Items</h1>
        <div className="flex items-center gap-3">
          <select
            value={sortOrder}
            onChange={(e) => setSortOrder(e.target.value as typeof sortOrder)}
            className="input w-auto text-xs"
          >
            <option value="newest">Newest first</option>
            <option value="oldest">Oldest first</option>
            <option value="name">By name</option>
          </select>
          <div className="flex gap-1 bg-archive-900 rounded-md p-0.5">
            {(["", "project", "file", "folder", "document"] as const).map((t) => (
              <button
                key={t}
                onClick={() => setTypeFilter(t)}
                className={`px-3 py-1.5 text-xs rounded ${
                  typeFilter === t
                    ? "bg-archive-700 text-archive-100"
                    : "text-archive-500 hover:text-archive-300"
                }`}
              >
                {t || "All"}
              </button>
            ))}
          </div>
        </div>
      </div>

      {loading ? (
        <div className="flex items-center gap-2 text-archive-500 py-8">
          <svg className="animate-spin h-4 w-4" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" fill="none" />
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
          </svg>
          <span className="text-sm">Loading...</span>
        </div>
      ) : items.length === 0 ? (
        <div className="text-center py-12">
          <p className="text-archive-500 text-sm">No items found.</p>
          <p className="text-archive-600 text-xs mt-1">Import items to see them here</p>
        </div>
      ) : (
        <>
          <p className="text-xs text-archive-500 mb-3">{items.length} item(s)</p>
          <div className="grid grid-cols-3 gap-3">
            {items.map((item) => (
              <ItemCard key={item.id} item={item} />
            ))}
          </div>
        </>
      )}
    </div>
  );
}
