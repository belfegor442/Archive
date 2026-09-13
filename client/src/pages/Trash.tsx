import { useEffect, useState } from "react";
import { api } from "../api";
import type { ArchiveItem } from "../types";

function formatDate(date: string): string {
  return new Date(date).toLocaleString("en-US", {
    year: "numeric", month: "short", day: "numeric", hour: "2-digit", minute: "2-digit",
  });
}

export default function Trash() {
  const [items, setItems] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.items.list({ status: "deleted" }).then(setItems).catch(console.error).finally(() => setLoading(false));
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
      <h1 className="text-2xl font-semibold text-archive-100 mb-2">Trash</h1>
      <p className="text-xs text-archive-500 mb-6">Items in trash are kept for recovery. They can be restored at any time.</p>

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
              <path strokeLinecap="round" strokeLinejoin="round" d="M14.74 9l-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 01-2.244 2.077H8.084a2.25 2.25 0 01-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 00-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 013.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 00-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 00-7.5 0" />
            </svg>
          </div>
          <p className="text-archive-500 text-sm">Trash is empty</p>
        </div>
      ) : (
        <div className="space-y-2">
          {items.map((item) => (
            <div key={item.id} className="card p-4 flex items-center justify-between group hover:border-archive-700 transition-colors">
              <div className="flex items-center gap-4 min-w-0">
                <div className="w-10 h-10 rounded-lg bg-archive-800 flex items-center justify-center flex-shrink-0">
                  <TypeIcon type={item.type} />
                </div>
                <div className="min-w-0">
                  <p className="text-sm font-medium text-archive-200 truncate">{item.name}</p>
                  <div className="flex items-center gap-3 mt-0.5">
                    <span className="text-xs text-archive-500 capitalize">{item.type}</span>
                    <span className="text-xs text-archive-600">|</span>
                    <span className="text-xs text-archive-500">{formatDate(item.lastModifiedAt)}</span>
                  </div>
                </div>
              </div>
              <button
                onClick={() => handleRestore(item.id)}
                className="btn-secondary text-xs opacity-0 group-hover:opacity-100 transition-opacity flex-shrink-0"
              >
                <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M9 15L3 9m0 0l6-6M3 9h12a6 6 0 010 12h-3" />
                </svg>
                Restore
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

function TypeIcon({ type }: { type: string }) {
  const colors: Record<string, string> = {
    project: "text-blue-400",
    file: "text-green-400",
    folder: "text-yellow-400",
    document: "text-purple-400",
  };
  return (
    <svg className={`w-5 h-5 ${colors[type] || "text-archive-400"}`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
      {type === "project" && <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12l8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />}
      {type === "file" && <path strokeLinecap="round" strokeLinejoin="round" d="M19.5 14.25v-2.625a3.375 3.375 0 00-3.375-3.375h-1.5A1.125 1.125 0 0113.5 7.125v-1.5a3.375 3.375 0 00-3.375-3.375H8.25m2.25 0H5.625c-.621 0-1.125.504-1.125 1.125v17.25c0 .621.504 1.125 1.125 1.125h12.75c.621 0 1.125-.504 1.125-1.125V11.25a9 9 0 00-9-9z" />}
      {type === "folder" && <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />}
      {type === "document" && <path strokeLinecap="round" strokeLinejoin="round" d="M19.5 14.25v-2.625a3.375 3.375 0 00-3.375-3.375h-1.5A1.125 1.125 0 0113.5 7.125v-1.5a3.375 3.375 0 00-3.375-3.375H8.25m0 12.75h7.5m-7.5 3H12M10.5 2.25H5.625c-.621 0-1.125.504-1.125 1.125v17.25c0 .621.504 1.125 1.125 1.125h12.75c.621 0 1.125-.504 1.125-1.125V11.25a9 9 0 00-9-9z" />}
    </svg>
  );
}
