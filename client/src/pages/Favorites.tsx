import { useEffect, useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem } from "../types";

export default function Favorites() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.items
      .list({ status: "favorite" })
      .then(setItems)
      .catch(console.error)
      .finally(() => setLoading(false));
  }, []);

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Favorites</h1>

      {loading ? (
        <p className="text-archive-500 text-sm">Loading...</p>
      ) : items.length === 0 ? (
        <p className="text-archive-500 text-sm">
          No favorite items yet. Star items to add them here.
        </p>
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
