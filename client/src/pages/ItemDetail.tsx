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
    year: "numeric", month: "short", day: "numeric", hour: "2-digit", minute: "2-digit",
  });
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
  return formatDate(date);
}

const typeColors: Record<string, string> = {
  project: "bg-blue-900/50 text-blue-300 border border-blue-800",
  file: "bg-green-900/50 text-green-300 border border-green-800",
  folder: "bg-yellow-900/50 text-yellow-300 border border-yellow-800",
  document: "bg-purple-900/50 text-purple-300 border border-purple-800",
};

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
  const [editing, setEditing] = useState(false);
  const [editName, setEditName] = useState("");
  const [editDesc, setEditDesc] = useState("");

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
        setEditName(itemData.name);
        setEditDesc(itemData.description || "");
      })
      .catch(console.error)
      .finally(() => setLoading(false));
  }, [id]);

  const handleDelete = async () => {
    if (!id) return;
    try {
      await api.items.delete(id);
      navigate("/items");
    } catch (err) { console.error(err); }
  };

  const handleRestore = async () => {
    if (!id) return;
    try {
      const updated = await api.items.restore(id);
      setItem(updated);
    } catch (err) { console.error(err); }
  };

  const handleToggleFavorite = async () => {
    if (!id || !item) return;
    try {
      const updated = await api.items.update(id, { isFavorite: !item.isFavorite });
      setItem(updated);
    } catch (err) { console.error(err); }
  };

  const handleSaveEdit = async () => {
    if (!id) return;
    try {
      const updated = await api.items.update(id, { name: editName, description: editDesc });
      setItem(updated);
      setEditing(false);
    } catch (err) { console.error(err); }
  };

  const handleAddNote = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!id || !newNote.trim()) return;
    try {
      const note = await api.items.addNote(id, newNote);
      setNotes((prev) => [note, ...prev]);
      setNewNote("");
    } catch (err) { console.error(err); }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="flex items-center gap-2 text-archive-500">
          <svg className="animate-spin h-4 w-4" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" fill="none" />
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
          </svg>
          <span className="text-sm">Loading...</span>
        </div>
      </div>
    );
  }

  if (!item) {
    return (
      <div className="text-center py-20">
        <p className="text-archive-500">Item not found.</p>
        <button onClick={() => navigate("/items")} className="btn-ghost mt-4 text-sm">
          Back to items
        </button>
      </div>
    );
  }

  const tabs = [
    { key: "info", label: "Information" },
    { key: "versions", label: `Versions (${versions.length})` },
    { key: "notes", label: `Notes (${notes.length})` },
    { key: "activity", label: `Activity (${activity.length})` },
  ] as const;

  return (
    <div>
      {/* Header */}
      <div className="flex items-start gap-4 mb-6">
        <button onClick={() => navigate(-1)} className="btn-ghost p-2 mt-1">
          <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M10.5 19.5L3 12m0 0l7.5-7.5M3 12h18" />
          </svg>
        </button>
        <div className="flex-1 min-w-0">
          {editing ? (
            <div className="space-y-2">
              <input
                type="text"
                value={editName}
                onChange={(e) => setEditName(e.target.value)}
                className="input text-lg font-semibold"
              />
              <input
                type="text"
                value={editDesc}
                onChange={(e) => setEditDesc(e.target.value)}
                className="input text-sm"
                placeholder="Description"
              />
              <div className="flex gap-2">
                <button onClick={handleSaveEdit} className="btn-primary text-xs">Save</button>
                <button onClick={() => setEditing(false)} className="btn-ghost text-xs">Cancel</button>
              </div>
            </div>
          ) : (
            <>
              <div className="flex items-center gap-3">
                <h1 className="text-2xl font-semibold text-archive-100 truncate">{item.name}</h1>
                <span className={`badge text-xs ${typeColors[item.type] || ""}`}>{item.type}</span>
              </div>
              {item.description && (
                <p className="text-sm text-archive-400 mt-1">{item.description}</p>
              )}
              <p className="text-xs text-archive-500 mt-1">
                Archived {timeAgo(item.archivedAt)}
              </p>
            </>
          )}
        </div>
        <div className="flex gap-2 flex-shrink-0">
          <button onClick={handleToggleFavorite} className={`btn-ghost p-2 ${item.isFavorite ? "text-yellow-400" : ""}`} title="Toggle favorite">
            <svg className="w-5 h-5" fill={item.isFavorite ? "currentColor" : "none"} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M11.48 3.499a.562.562 0 011.04 0l2.125 5.111a.563.563 0 00.475.345l5.518.442c.499.04.701.663.321.988l-4.204 3.602a.563.563 0 00-.182.557l1.285 5.385a.562.562 0 01-.84.61l-4.725-2.885a.563.563 0 00-.586 0L6.982 20.54a.562.562 0 01-.84-.61l1.285-5.386a.562.562 0 00-.182-.557l-4.204-3.602a.563.563 0 01.321-.988l5.518-.442a.563.563 0 00.475-.345L11.48 3.5z" />
            </svg>
          </button>
          <button onClick={() => setEditing(true)} className="btn-ghost p-2" title="Edit">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M16.862 4.487l1.687-1.688a1.875 1.875 0 112.652 2.652L10.582 16.07a4.5 4.5 0 01-1.897 1.13L6 18l.8-2.685a4.5 4.5 0 011.13-1.897l8.932-8.931zm0 0L19.5 7.125M18 14v4.75A2.25 2.25 0 0115.75 21H5.25A2.25 2.25 0 013 18.75V8.25A2.25 2.25 0 015.25 6H10" />
            </svg>
          </button>
          {item.status === "deleted" ? (
            <button onClick={handleRestore} className="btn-secondary text-xs">
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M9 15L3 9m0 0l6-6M3 9h12a6 6 0 010 12h-3" />
              </svg>
              Restore
            </button>
          ) : (
            <button onClick={() => setConfirmDelete(true)} className="btn-danger text-xs">
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M14.74 9l-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 01-2.244 2.077H8.084a2.25 2.25 0 01-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 00-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 013.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 00-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 00-7.5 0" />
              </svg>
              Delete
            </button>
          )}
        </div>
      </div>

      {/* Tabs */}
      <div className="flex gap-1 mb-6 border-b border-archive-800">
        {tabs.map((tab) => (
          <button
            key={tab.key}
            onClick={() => setActiveTab(tab.key)}
            className={`px-4 py-2.5 text-sm font-medium border-b-2 transition-colors ${
              activeTab === tab.key
                ? "border-archive-200 text-archive-100"
                : "border-transparent text-archive-500 hover:text-archive-300"
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab Content */}
      {activeTab === "info" && (
        <div className="grid grid-cols-2 gap-8">
          <div className="space-y-5">
            <SectionTitle>General</SectionTitle>
            <InfoRow label="Type" value={item.type} />
            <InfoRow label="Status" value={item.status} />
            <InfoRow label="Size" value={formatBytes(item.size)} />
            <InfoRow label="Files" value={String(item.fileCount)} />
            <InfoRow label="Version" value={`v${item.currentVersion}`} />
            {item.category && (
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 rounded-full" style={{ backgroundColor: item.category.color }} />
                <span className="text-sm text-archive-300">{item.category.name}</span>
              </div>
            )}
          </div>
          <div className="space-y-5">
            <SectionTitle>Dates</SectionTitle>
            <InfoRow label="Created" value={formatDate(item.createdAt)} />
            <InfoRow label="Archived" value={formatDate(item.archivedAt)} />
            <InfoRow label="Last Modified" value={formatDate(item.lastModifiedAt)} />
          </div>
          <div className="col-span-2 space-y-5">
            <SectionTitle>Paths</SectionTitle>
            <InfoRow label="Original Path" value={item.originalPath} mono />
            <InfoRow label="Storage Path" value={item.storagePath} mono />
            {item.checksum && <InfoRow label="Checksum (SHA-256)" value={item.checksum} mono />}
          </div>
          {item.tags && item.tags.length > 0 && (
            <div className="col-span-2 space-y-3">
              <SectionTitle>Tags</SectionTitle>
              <div className="flex gap-2 flex-wrap">
                {item.tags.map((tag) => (
                  <span key={tag.id} className="badge bg-archive-800 text-archive-400 border border-archive-700">
                    {tag.name}
                  </span>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {activeTab === "versions" && (
        <div>
          {versions.length === 0 ? (
            <p className="text-archive-500 text-sm">No versions recorded.</p>
          ) : (
            <div className="space-y-2">
              {versions.map((v) => (
                <div key={v.id} className="card p-4 flex items-center justify-between">
                  <div className="flex items-center gap-4">
                    <div className={`w-10 h-10 rounded-lg flex items-center justify-center text-sm font-mono ${
                      v.versionNumber === item.currentVersion
                        ? "bg-archive-200 text-archive-950 font-semibold"
                        : "bg-archive-800 text-archive-400"
                    }`}>
                      v{v.versionNumber}
                    </div>
                    <div>
                      <p className="text-sm text-archive-200">
                        Version {v.versionNumber}
                        {v.versionNumber === item.currentVersion && (
                          <span className="badge bg-green-900/50 text-green-300 border border-green-800 ml-2 text-xs">current</span>
                        )}
                      </p>
                      <p className="text-xs text-archive-500 mt-0.5">{formatDate(v.createdAt)}</p>
                      {v.notes && <p className="text-xs text-archive-400 mt-1">{v.notes}</p>}
                    </div>
                  </div>
                  <div className="text-right">
                    <p className="text-xs text-archive-500">{formatBytes(v.size)}</p>
                    {v.checksum && (
                      <p className="text-xs text-archive-600 font-mono mt-0.5">{v.checksum.substring(0, 12)}...</p>
                    )}
                  </div>
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
            <button type="submit" className="btn-primary" disabled={!newNote.trim()}>
              Add Note
            </button>
          </form>
          {notes.length === 0 ? (
            <p className="text-archive-500 text-sm">No notes yet. Add one above.</p>
          ) : (
            <div className="space-y-2">
              {notes.map((note) => (
                <div key={note.id} className="card p-4">
                  <p className="text-sm text-archive-300">{note.content}</p>
                  <p className="text-xs text-archive-600 mt-2">{formatDate(note.createdAt)}</p>
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
            <div className="relative">
              <div className="absolute left-4 top-0 bottom-0 w-px bg-archive-800" />
              <div className="space-y-4">
                {activity.map((a) => (
                  <div key={a.id} className="flex gap-4 relative">
                    <div className="w-8 h-8 rounded-full bg-archive-800 flex items-center justify-center flex-shrink-0 z-10">
                      <ActivityIcon action={a.action} />
                    </div>
                    <div className="pt-1">
                      <p className="text-sm text-archive-200">{a.action}</p>
                      {a.details && <p className="text-xs text-archive-500 mt-0.5">{a.details}</p>}
                      <p className="text-xs text-archive-600 mt-1">{timeAgo(a.createdAt)}</p>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Delete Confirmation Modal */}
      {confirmDelete && (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
          <div className="bg-archive-900 border border-archive-800 rounded-lg p-5 w-full max-w-sm mx-4">
            <h3 className="text-lg font-semibold text-archive-100 mb-2">Move to Trash?</h3>
            <p className="text-sm text-archive-400 mb-5">
              This will move "{item.name}" to trash. You can restore it later from the Trash section.
            </p>
            <div className="flex justify-end gap-2">
              <button onClick={() => setConfirmDelete(false)} className="btn-secondary">Cancel</button>
              <button onClick={handleDelete} className="btn-danger">Move to Trash</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function SectionTitle({ children }: { children: React.ReactNode }) {
  return <h3 className="text-xs font-medium text-archive-500 uppercase tracking-wider">{children}</h3>;
}

function InfoRow({ label, value, mono }: { label: string; value: string; mono?: boolean }) {
  return (
    <div>
      <p className="text-xs text-archive-600 mb-0.5">{label}</p>
      <p className={`text-sm text-archive-300 ${mono ? "font-mono text-xs break-all bg-archive-950 px-2 py-1 rounded" : ""}`}>
        {value}
      </p>
    </div>
  );
}

function ActivityIcon({ action }: { action: string }) {
  const iconClass = "w-3.5 h-3.5";
  if (action.includes("Import")) return <svg className={`${iconClass} text-green-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M12 16.5V9.75m0 0l3 3m-3-3l-3 3M6.75 19.5a4.5 4.5 0 01-1.41-8.775 5.25 5.25 0 0110.233-2.33 3 3 0 013.758 3.848A3.752 3.752 0 0118 19.5H6.75z" /></svg>;
  if (action.includes("Delete")) return <svg className={`${iconClass} text-red-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M14.74 9l-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 01-2.244 2.077H8.084a2.25 2.25 0 01-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 00-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 013.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 00-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 00-7.5 0" /></svg>;
  if (action.includes("Restore")) return <svg className={`${iconClass} text-blue-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M9 15L3 9m0 0l6-6M3 9h12a6 6 0 010 12h-3" /></svg>;
  if (action.includes("Version")) return <svg className={`${iconClass} text-purple-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M7.5 21L3 16.5m0 0L7.5 12M3 16.5h13.5m0-13.5L21 7.5m0 0L16.5 12M21 7.5H7.5" /></svg>;
  if (action.includes("Note")) return <svg className={`${iconClass} text-yellow-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M16.862 4.487l1.687-1.688a1.875 1.875 0 112.652 2.652L10.582 16.07a4.5 4.5 0 01-1.897 1.13L6 18l.8-2.685a4.5 4.5 0 011.13-1.897l8.932-8.931zm0 0L19.5 7.125" /></svg>;
  return <svg className={`${iconClass} text-archive-400`} fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}><path strokeLinecap="round" strokeLinejoin="round" d="M12 6v6h4.5m4.5 0a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>;
}
