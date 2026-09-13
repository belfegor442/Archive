import { useState } from "react";
import { api } from "../api";

interface ImportModalProps {
  onClose: () => void;
  onComplete: () => void;
}

export default function ImportModal({ onClose, onComplete }: ImportModalProps) {
  const [paths, setPaths] = useState("");
  const [description, setDescription] = useState("");
  const [tags, setTags] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<{ imported: number; errors: { path: string; error: string }[] } | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const pathList = paths
      .split("\n")
      .map((p) => p.trim())
      .filter(Boolean);

    if (pathList.length === 0) {
      setError("Enter at least one path");
      return;
    }

    setLoading(true);
    setError(null);
    setResult(null);

    try {
      const tagList = tags
        .split(",")
        .map((t) => t.trim())
        .filter(Boolean);

      const importResult = await api.items.create({
        paths: pathList,
        description,
        tags: tagList.length > 0 ? tagList : undefined,
      });

      setResult({
        imported: importResult.items.length,
        errors: importResult.errors,
      });

      if (importResult.items.length > 0) {
        setTimeout(() => onComplete(), 1500);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Import failed");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
      <div className="bg-archive-900 border border-archive-800 rounded-lg w-full max-w-lg mx-4">
        <div className="flex items-center justify-between p-4 border-b border-archive-800">
          <h2 className="text-lg font-semibold text-archive-100">
            Import to Archive
          </h2>
          <button onClick={onClose} className="btn-ghost p-1">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>

        <form onSubmit={handleSubmit} className="p-4 space-y-4">
          <div>
            <label className="block text-sm font-medium text-archive-300 mb-1">
              Paths (one per line)
            </label>
            <textarea
              value={paths}
              onChange={(e) => setPaths(e.target.value)}
              className="input h-32 resize-none font-mono text-xs"
              placeholder="/path/to/project&#10;/path/to/file.zip&#10;/path/to/folder"
              disabled={loading}
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-archive-300 mb-1">
              Description (optional)
            </label>
            <input
              type="text"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              className="input"
              placeholder="Brief description of what is being archived"
              disabled={loading}
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-archive-300 mb-1">
              Tags (comma separated, optional)
            </label>
            <input
              type="text"
              value={tags}
              onChange={(e) => setTags(e.target.value)}
              className="input"
              placeholder="javascript, react, archived-2024"
              disabled={loading}
            />
          </div>

          {error && (
            <div className="bg-red-900/30 border border-red-800 rounded p-3">
              <p className="text-sm text-red-400">{error}</p>
            </div>
          )}

          {result && (
            <div className="bg-archive-800 border border-archive-700 rounded p-3 space-y-1">
              <p className="text-sm text-green-400">
                Successfully imported {result.imported} item(s)
              </p>
              {result.errors.length > 0 && (
                <div>
                  <p className="text-sm text-yellow-400">
                    {result.errors.length} path(s) failed:
                  </p>
                  {result.errors.map((err, i) => (
                    <p key={i} className="text-xs text-archive-500 ml-2">
                      {err.path}: {err.error}
                    </p>
                  ))}
                </div>
              )}
            </div>
          )}

          <div className="flex justify-end gap-2 pt-2">
            <button
              type="button"
              onClick={onClose}
              className="btn-secondary"
              disabled={loading}
            >
              {result ? "Close" : "Cancel"}
            </button>
            {!result && (
              <button
                type="submit"
                disabled={loading}
                className="btn-primary"
              >
                {loading ? "Importing..." : "Import"}
              </button>
            )}
          </div>
        </form>
      </div>
    </div>
  );
}
