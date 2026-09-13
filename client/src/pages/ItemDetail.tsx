import { useEffect, useState } from "react";
import { useParams, useNavigate } from "react-router-dom";
import { api } from "../api";
import type { ArchiveItem, Version, Note, Activity } from "../types";

function formatBytes(bytes: number): string {
  if (bytes === 0) return "0 B";
  const k = 1024;
  const sizes = ["B", "KB", "MB", "GB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
}

function formatDate(date: string): string {
  return new Date(date).toLocaleString("en-US", {
    year: "numeric",
    month: "short",
    day: "numeric",
    hour: "2-digit",
    minute: "2-digit",
  });
}

export default function ItemDetail() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const [item, setItem] = useState<ArchiveItem | null>(null);
  const [versions, setVersions] = useState<Version[]>([]);
  const [notes, setNotes] = useState<Note[]>([]);
  const [activity, setActivity] = useState<Activity[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeTab, setActiveTab] = useState<"info" | "versions" | "notes" | "activity">("info");
  const [newNote, setNewNote] = useState("");
  const [confirmDelete, setConfirmDelete] = useState(false);

  useEffect(() => {
    if (!id) return;
    Promise.all([
      api.items.get(id),
      api.items.versions(id),
      api.items.notes(id),
      api.items.activity(id),
    ])
      .then(([itemData, versionsData, notesData, activityData]) => {
        setItem(itemData);
        setVersions(versionsData);
        setNotes(notesData);
        setActivity(activityData);
      })
      .catch(console.error)
      .finally(() => setLoading(false));
  }, [id]);

  const handleDelete = async () => {
    if (!id) return;
    try {
      await api.items.delete(id);
      navigate("/items");
    } catch (err) {
      console.error(err);
    }
  };

  const handleRestore = async () => {
    if (!id) return;
    try {
      const updated = await api.items.restore(id);
      setItem(updated);
    } catch (err) {
      console.error(err);
    }
  };

  const handleToggleFavorite = async () => {
    if (!id || !item) return;
    try {
      const updated = await api.items.update(id, {
        isFavorite: !item.isFavorite,
      });
      setItem(updated);
    } catch (err) {
      console.error(err);
    }
  };

  const handleAddNote = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!id || !newNote.trim()) return;
    try {
      const note = await api.items.addNote(id, newNote);
      setNotes((prev) => [...prev, note]);
      setNewNote("");
    } catch (err) {
      console.error(err);
    }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <p className="text-archive-500">Loading...</p>
      </div>
    );
  }

  if (!item) {
    return (
      <div className="text-center py-20">
        <p className="text-archive-500">Item not found.</p>
      </div>
    );
  }

  const tabs = [
    { key: "info", label: "Information" },
    { key: "versions", label: `Versions (${versions.length})` },
    { key: "notes", label: `Notes (${notes.length})` },
    { key: "activity", label: "Activity" },
  ] as const;

  return (
    <div>
      <div className="flex items-center gap-3 mb-6">
        <button onClick={() => navigate(-1)} className="btn-ghost p-1">
          <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M10.5 19.5L3 12m0 0l7.5-7.5M3 12h18" />
          </svg>
        </button>
        <div className="flex-1">
          <h1 className="text-2xl font-semibold text-archive-100">{item.name}</h1>
          <p className="text-xs text-archive-500 mt-1">
            Archived {formatDate(item.archivedAt)}
          </p>
        </div>
        <div className="flex gap-2">
          <button
            onClick={handleToggleFavorite}
            className={`btn-ghost ${item.isFavorite ? "text-yellow-400" : ""}`}
          >
            <svg className="w-4 h-4" fill={item.isFavorite ? "currentColor" : "none"} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M11.48 3.499a.562.562 0 011.04 0l2.125 5.111a.563.563 0 00.475.345l5.518.442c.499.04.701.663.321.988l-4.204 3.602a.563.563 0 00-.182.557l1.285 5.385a.562.562 0 01-.84.61l-4.725-2.885a.563.563 0 00-.586 0L6.982 20.54a.562.562 0 01-.84-.61l1.285-5.386a.562.562 0 00-.182-.557l-4.204-3.602a.563.563 0 01.321-.988l5.518-.442a.563.563 0 00.475-.345L11.48 3.5z" />
            </svg>
          </button>
          {item.status === "deleted" ? (
            <button onClick={handleRestore} className="btn-secondary text-xs">
              Restore
            </button>
          ) : (
            <button
              onClick={() => setConfirmDelete(true)}
              className="btn-danger text-xs"
            >
              Delete
            </button>
          )}
        </div>
      </div>

      <div className="flex gap-1 mb-6 border-b border-archive-800">
        {tabs.map((tab) => (
          <button
            key={tab.key}
            onClick={() => setActiveTab(tab.key)}
            className={`px-4 py-2 text-sm font-medium border-b-2 transition-colors ${
              activeTab === tab.key
                ? "border-archive-200 text-archive-100"
                : "border-transparent text-archive-500 hover:text-archive-300"
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {activeTab === "info" && (
        <div className="grid grid-cols-2 gap-6">
          <div className="space-y-4">
            <InfoRow label="Type" value={item.type} />
            <InfoRow label="Status" value={item.status} />
            <InfoRow label="Size" value={formatBytes(item.size)} />
            <InfoRow label="Files" value={String(item.fileCount)} />
            <InfoRow label="Created" value={formatDate(item.createdAt)} />
            <InfoRow label="Archived" value={formatDate(item.archivedAt)} />
            <InfoRow label="Modified" value={formatDate(item.lastModifiedAt)} />
            <InfoRow label="Version" value={`v${item.currentVersion}`} />
          </div>
          <div className="space-y-4">
            <InfoRow label="Original Path" value={item.originalPath} mono />
            <InfoRow label="Storage Path" value={item.storagePath} mono />
            <InfoRow label="Checksum" value={item.checksum} mono />
            {item.description && (
              <div>
                <p className="text-xs text-archive-500 mb-1">Description</p>
                <p className="text-sm text-archive-300">{item.description}</p>
              </div>
            )}
            {item.tags && item.tags.length > 0 && (
              <div>
                <p className="text-xs text-archive-500 mb-1">Tags</p>
                <div className="flex gap-1 flex-wrap">
                  {item.tags.map((tag) => (
                    <span
                      key={tag.id}
                      className="badge bg-archive-800 text-archive-400 border border-archive-700"
                    >
                      {tag.name}
                    </span>
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {activeTab === "versions" && (
        <div>
          {versions.length === 0 ? (
            <p className="text-archive-500 text-sm">No versions recorded.</p>
          ) : (
            <div className="space-y-2">
              {versions.map((v) => (
                <div key={v.id} className="card p-3 flex items-center justify-between">
                  <div>
                    <span className="text-sm font-medium text-archive-200">
                      v{v.versionNumber}
                    </span>
                    <span className="text-xs text-archive-500 ml-3">
                      {formatDate(v.createdAt)}
                    </span>
                    {v.notes && (
                      <p className="text-xs text-archive-400 mt-1">{v.notes}</p>
                    )}
                  </div>
                  <span className="text-xs text-archive-500">
                    {formatBytes(v.size)}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {activeTab === "notes" && (
        <div>
          <form onSubmit={handleAddNote} className="mb-4 flex gap-2">
            <input
              type="text"
              value={newNote}
              onChange={(e) => setNewNote(e.target.value)}
              className="input flex-1"
              placeholder="Add a note..."
            />
            <button type="submit" className="btn-primary">
              Add
            </button>
          </form>
          {notes.length === 0 ? (
            <p className="text-archive-500 text-sm">No notes yet.</p>
          ) : (
            <div className="space-y-2">
              {notes.map((note) => (
                <div key={note.id} className="card p-3">
                  <p className="text-sm text-archive-300">{note.content}</p>
                  <p className="text-xs text-archive-600 mt-1">
                    {formatDate(note.createdAt)}
                  </p>
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {activeTab === "activity" && (
        <div>
          {activity.length === 0 ? (
            <p className="text-archive-500 text-sm">No activity recorded.</p>
          ) : (
            <div className="space-y-2">
              {activity.map((a) => (
                <div key={a.id} className="card p-3 flex items-start gap-3">
                  <div className="w-2 h-2 rounded-full bg-archive-600 mt-1.5 flex-shrink-0" />
                  <div>
                    <p className="text-sm text-archive-300">{a.action}</p>
                    {a.details && (
                      <p className="text-xs text-archive-500 mt-0.5">
                        {a.details}
                      </p>
                    )}
                    <p className="text-xs text-archive-600 mt-1">
                      {formatDate(a.createdAt)}
                    </p>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {confirmDelete && (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
          <div className="bg-archive-900 border border-archive-800 rounded-lg p-4 w-full max-w-sm mx-4">
            <h3 className="text-lg font-semibold text-archive-100 mb-2">
              Confirm Delete
            </h3>
            <p className="text-sm text-archive-400 mb-4">
              This will move "{item.name}" to trash. You can restore it later.
            </p>
            <div className="flex justify-end gap-2">
              <button
                onClick={() => setConfirmDelete(false)}
                className="btn-secondary"
              >
                Cancel
              </button>
              <button onClick={handleDelete} className="btn-danger">
                Delete
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function InfoRow({
  label,
  value,
  mono,
}: {
  label: string;
  value: string;
  mono?: boolean;
}) {
  return (
    <div>
      <p className="text-xs text-archive-500 mb-1">{label}</p>
      <p className={`text-sm text-archive-300 ${mono ? "font-mono text-xs break-all" : ""}`}>
        {value}
      </p>
    </div>
  );
}
