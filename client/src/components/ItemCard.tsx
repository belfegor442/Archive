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

function timeAgo(date: string): string {
  const seconds = Math.floor((Date.now() - new Date(date).getTime()) / 1000);
  if (seconds < 60) return "just now";
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}h ago`;
  const days = Math.floor(hours / 24);
  if (days < 30) return `${days}d ago`;
  const months = Math.floor(days / 30);
  return `${months}mo ago`;
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
      className="card p-4 cursor-pointer hover:border-archive-700 transition-all duration-150 group"
    >
      <div className="flex items-start justify-between mb-2">
        <h3 className="text-sm font-medium text-archive-100 truncate group-hover:text-archive-50 transition-colors">
          {item.name}
        </h3>
        <div className="flex items-center gap-1.5 ml-2 flex-shrink-0">
          {item.isFavorite && (
            <svg className="w-3.5 h-3.5 text-yellow-400" fill="currentColor" viewBox="0 0 24 24">
              <path d="M11.48 3.499a.562.562 0 011.04 0l2.125 5.111a.563.563 0 00.475.345l5.518.442c.499.04.701.663.321.988l-4.204 3.602a.563.563 0 00-.182.557l1.285 5.385a.562.562 0 01-.84.61l-4.725-2.885a.563.563 0 00-.586 0L6.982 20.54a.562.562 0 01-.84-.61l1.285-5.386a.562.562 0 00-.182-.557l-4.204-3.602a.563.563 0 01.321-.988l5.518-.442a.563.563 0 00.475-.345L11.48 3.5z" />
            </svg>
          )}
          <span className={`badge text-[10px] ${typeColors[item.type] || ""}`}>
            {item.type}
          </span>
        </div>
      </div>

      {item.description && (
        <p className="text-xs text-archive-500 mb-3 line-clamp-2 leading-relaxed">
          {item.description}
        </p>
      )}

      <div className="flex items-center gap-3 text-[11px] text-archive-600">
        <span className="flex items-center gap-1">
          <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M20.25 7.5l-.625 10.632a2.25 2.25 0 01-2.247 2.118H6.622a2.25 2.25 0 01-2.247-2.118L3.75 7.5M10 11.25h4M3.375 7.5h17.25c.621 0 1.125-.504 1.125-1.125v-1.5c0-.621-.504-1.125-1.125-1.125H3.375c-.621 0-1.125.504-1.125 1.125v1.5c0 .621.504 1.125 1.125 1.125z" />
          </svg>
          {formatBytes(item.size)}
        </span>
        <span className="flex items-center gap-1">
          <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
          </svg>
          {item.fileCount} {item.fileCount === 1 ? "file" : "files"}
        </span>
        <span className="flex items-center gap-1">
          <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M12 6v6h4.5m4.5 0a9 9 0 11-18 0 9 9 0 0118 0z" />
          </svg>
          {timeAgo(item.archivedAt)}
        </span>
      </div>

      {item.tags && item.tags.length > 0 && (
        <div className="flex gap-1 mt-2.5 flex-wrap">
          {item.tags.slice(0, 3).map((tag) => (
            <span key={tag.id} className="badge bg-archive-800/80 text-archive-400 text-[10px] border border-archive-700/50">
              {tag.name}
            </span>
          ))}
          {item.tags.length > 3 && (
            <span className="badge bg-archive-800/80 text-archive-500 text-[10px]">
              +{item.tags.length - 3}
            </span>
          )}
        </div>
      )}
    </div>
  );
}
