import { useEffect, useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem } from "../types";

export default function Favorites() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.items.list({ status: "favorite" }).then(setItems).catch(console.error).finally(() => setLoading(false));
  }, []);

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Favorites</h1>

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
          <div className="inline-flex items-center justify-center w-12 h-12 rounded-full bg-archive-900 mb-3">
            <svg className="w-6 h-6 text-archive-600" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M11.48 3.499a.562.562 0 011.04 0l2.125 5.111a.563.563 0 00.475.345l5.518.442c.499.04.701.663.321.988l-4.204 3.602a.563.563 0 00-.182.557l1.285 5.385a.562.562 0 01-.84.61l-4.725-2.885a.563.563 0 00-.586 0L6.982 20.54a.562.562 0 01-.84-.61l1.285-5.386a.562.562 0 00-.182-.557l-4.204-3.602a.563.563 0 01.321-.988l5.518-.442a.563.563 0 00.475-.345L11.48 3.5z" />
            </svg>
          </div>
          <p className="text-archive-500 text-sm mb-1">No favorites yet</p>
          <p className="text-archive-600 text-xs">Star items to add them here</p>
        </div>
      ) : (
        <>
          <p className="text-xs text-archive-500 mb-3">{items.length} favorite(s)</p>
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
