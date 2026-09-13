import { useEffect, useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { DashboardStats } from "../types";

function formatBytes(bytes: number): string {
  if (bytes === 0) return "0 B";
  const k = 1024;
  const sizes = ["B", "KB", "MB", "GB", "TB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
}

export default function Dashboard() {
  const [stats, setStats] = useState<DashboardStats | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api.stats().then(setStats).catch(console.error).finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <p className="text-archive-500">Loading...</p>
      </div>
    );
  }

  if (!stats) {
    return (
      <div className="text-center py-20">
        <h2 className="text-xl font-semibold text-archive-300 mb-2">
          Welcome to Archive
        </h2>
        <p className="text-archive-500 text-sm">
          Import your first items to get started.
        </p>
      </div>
    );
  }

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Dashboard</h1>

      <div className="grid grid-cols-4 gap-4 mb-8">
        <StatCard label="Total Items" value={stats.totalItems} />
        <StatCard label="Archived" value={stats.archivedItems} />
        <StatCard label="Favorites" value={stats.favoriteItems} />
        <StatCard label="Total Size" value={formatBytes(stats.totalSize)} />
      </div>

      {stats.categoryCounts.length > 0 && (
        <div className="mb-8">
          <h2 className="text-sm font-medium text-archive-400 mb-3">
            Categories
          </h2>
          <div className="flex gap-3 flex-wrap">
            {stats.categoryCounts.map((cat) => (
              <div key={cat.categoryId} className="card px-3 py-2 flex items-center gap-2">
                <div
                  className="w-2 h-2 rounded-full"
                  style={{ backgroundColor: cat.color }}
                />
                <span className="text-sm text-archive-300">{cat.name}</span>
                <span className="text-xs text-archive-500">{cat.count}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {stats.recentItems.length > 0 && (
        <div>
          <h2 className="text-sm font-medium text-archive-400 mb-3">
            Recently Archived
          </h2>
          <div className="grid grid-cols-3 gap-3">
            {stats.recentItems.map((item) => (
              <ItemCard key={item.id} item={item} />
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

function StatCard({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="card p-4">
      <p className="text-xs text-archive-500 mb-1">{label}</p>
      <p className="text-2xl font-semibold text-archive-100">{value}</p>
    </div>
  );
}
