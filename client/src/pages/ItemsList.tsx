import { useEffect, useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem, ItemType } from "../types";

export default function ItemsList() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [typeFilter, setTypeFilter] = useState<ItemType | "">("");

  useEffect(() => {
    setLoading(true);
    api.items
      .list({ status: "archived", type: typeFilter || undefined })
      .then(setItems)
      .catch(console.error)
      .finally(() => setLoading(false));
  }, [typeFilter]);

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-archive-100">All Items</h1>
        <div className="flex gap-2">
          {(["", "project", "file", "folder", "document"] as const).map((t) => (
            <button
              key={t}
              onClick={() => setTypeFilter(t)}
              className={`btn text-xs ${
                typeFilter === t ? "bg-archive-700 text-archive-100" : "btn-ghost"
              }`}
            >
              {t || "All"}
            </button>
          ))}
        </div>
      </div>

      {loading ? (
        <p className="text-archive-500 text-sm">Loading...</p>
      ) : items.length === 0 ? (
        <p className="text-archive-500 text-sm">No items found.</p>
      ) : (
        <div className="grid grid-cols-3 gap-3">
          {items.map((item) => (
            <ItemCard key={item.id} item={item} />
          ))}
        </div>
      )}
    </div>
  );
}
