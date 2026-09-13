import type { ArchiveItem } from "../types";
import { useNavigate } from "react-router-dom";

interface ItemCardProps {
  item: ArchiveItem;
}

function formatBytes(bytes: number): string {
  if (bytes === 0) return "0 B";
  const k = 1024;
  const sizes = ["B", "KB", "MB", "GB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
}

function formatDate(date: string): string {
  return new Date(date).toLocaleDateString("en-US", {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
}

const typeColors: Record<string, string> = {
  project: "bg-blue-900/50 text-blue-300 border border-blue-800",
  file: "bg-green-900/50 text-green-300 border border-green-800",
  folder: "bg-yellow-900/50 text-yellow-300 border border-yellow-800",
  document: "bg-purple-900/50 text-purple-300 border border-purple-800",
};

export default function ItemCard({ item }: ItemCardProps) {
  const navigate = useNavigate();

  return (
    <div
      onClick={() => navigate(`/items/${item.id}`)}
      className="card p-4 cursor-pointer hover:border-archive-700 transition-colors"
    >
      <div className="flex items-start justify-between mb-2">
        <h3 className="text-sm font-medium text-archive-100 truncate">
          {item.name}
        </h3>
        <span className={`badge ml-2 flex-shrink-0 ${typeColors[item.type] || ""}`}>
          {item.type}
        </span>
      </div>

      {item.description && (
        <p className="text-xs text-archive-500 mb-3 line-clamp-2">
          {item.description}
        </p>
      )}

      <div className="flex items-center gap-4 text-xs text-archive-500">
        <span>{formatBytes(item.size)}</span>
        <span>{item.fileCount} files</span>
        <span>{formatDate(item.archivedAt)}</span>
      </div>

      {item.tags && item.tags.length > 0 && (
        <div className="flex gap-1 mt-2 flex-wrap">
          {item.tags.slice(0, 3).map((tag) => (
            <span
              key={tag.id}
              className="badge bg-archive-800 text-archive-400 border border-archive-700"
            >
              {tag.name}
            </span>
          ))}
          {item.tags.length > 3 && (
            <span className="badge bg-archive-800 text-archive-500">
              +{item.tags.length - 3}
            </span>
          )}
        </div>
      )}
    </div>
  );
}
