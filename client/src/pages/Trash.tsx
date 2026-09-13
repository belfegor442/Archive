import { useEffect, useState } from "react";
import { api } from "../api";
import type { ArchiveItem } from "../types";

export default function Trash() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.items
      .list({ status: "deleted" })
      .then(setItems)
      .catch(console.error)
      .finally(() => setLoading(false));
  }, []);

  const handleRestore = async (id: string) => {
    try {
      await api.items.restore(id);
      setItems((prev) => prev.filter((i) => i.id !== id));
    } catch (err) {
      console.error(err);
    }
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Trash</h1>

      {loading ? (
        <p className="text-archive-500 text-sm">Loading...</p>
      ) : items.length === 0 ? (
        <p className="text-archive-500 text-sm">Trash is empty.</p>
      ) : (
        <div className="space-y-2">
          {items.map((item) => (
            <div key={item.id} className="card p-3 flex items-center justify-between">
              <div className="flex-1 min-w-0">
                <p className="text-sm font-medium text-archive-200 truncate">
                  {item.name}
                </p>
                <p className="text-xs text-archive-500">
                  Deleted {new Date(item.lastModifiedAt).toLocaleDateString()}
                </p>
              </div>
              <button
                onClick={() => handleRestore(item.id)}
                className="btn-secondary text-xs ml-4"
              >
                Restore
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
