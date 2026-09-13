import { useEffect, useState } from "react";
import { api } from "../api";
import type { Category } from "../types";

export default function Categories() {
  const [categories, setCategories] = useState<Category[]>([]);
  const [loading, setLoading] = useState(true);
  const [showForm, setShowForm] = useState(false);
  const [name, setName] = useState("");
  const [description, setDescription] = useState("");
  const [color, setColor] = useState("#6366f1");
  const [editingId, setEditingId] = useState<string | null>(null);

  useEffect(() => {
    api.categories.list().then(setCategories).catch(console.error).finally(() => setLoading(false));
  }, []);

  const handleCreate = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim()) return;
    try {
      if (editingId) {
        const cat = await api.categories.update(editingId, { name, description, color });
        setCategories((prev) => prev.map((c) => (c.id === editingId ? cat : c)));
      } else {
        const cat = await api.categories.create({ name, description, color });
        setCategories((prev) => [...prev, cat]);
      }
      setName("");
      setDescription("");
      setColor("#6366f1");
      setEditingId(null);
      setShowForm(false);
    } catch (err) {
      console.error(err);
    }
  };

  const handleEdit = (cat: Category) => {
    setName(cat.name);
    setDescription(cat.description);
    setColor(cat.color);
    setEditingId(cat.id);
    setShowForm(true);
  };

  const handleDelete = async (id: string) => {
    if (!confirm("Delete this category? Items in this category will become uncategorized.")) return;
    try {
      await api.categories.delete(id);
      setCategories((prev) => prev.filter((c) => c.id !== id));
    } catch (err) {
      console.error(err);
    }
  };

  const handleCancel = () => {
    setName("");
    setDescription("");
    setColor("#6366f1");
    setEditingId(null);
    setShowForm(false);
  };

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-archive-100">Categories</h1>
        <button
          onClick={() => { handleCancel(); setShowForm(!showForm); }}
          className={showForm ? "btn-ghost" : "btn-primary"}
        >
          {showForm ? "Cancel" : "New Category"}
        </button>
      </div>

      {showForm && (
        <form onSubmit={handleCreate} className="card p-4 mb-6 space-y-3 max-w-md">
          <h3 className="text-sm font-medium text-archive-200 mb-2">
            {editingId ? "Edit Category" : "New Category"}
          </h3>
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            className="input"
            placeholder="Category name"
            autoFocus
          />
          <input
            type="text"
            value={description}
            onChange={(e) => setDescription(e.target.value)}
            className="input"
            placeholder="Description (optional)"
          />
          <div className="flex items-center gap-3">
            <label className="text-xs text-archive-500">Color</label>
            <input
              type="color"
              value={color}
              onChange={(e) => setColor(e.target.value)}
              className="w-8 h-8 rounded border border-archive-700 bg-transparent cursor-pointer"
            />
            <span className="text-xs text-archive-500 font-mono">{color}</span>
          </div>
          <div className="flex gap-2">
            <button type="submit" className="btn-primary">
              {editingId ? "Save Changes" : "Create"}
            </button>
            <button type="button" onClick={handleCancel} className="btn-ghost">
              Cancel
            </button>
          </div>
        </form>
      )}

      {loading ? (
        <div className="flex items-center gap-2 text-archive-500 py-8">
          <svg className="animate-spin h-4 w-4" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" fill="none" />
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
          </svg>
          <span className="text-sm">Loading...</span>
        </div>
      ) : categories.length === 0 ? (
        <div className="text-center py-12">
          <div className="inline-flex items-center justify-center w-12 h-12 rounded-full bg-archive-900 mb-3">
            <svg className="w-6 h-6 text-archive-600" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M9.568 3H5.25A2.25 2.25 0 003 5.25v4.318c0 .597.237 1.17.659 1.591l9.581 9.581c.699.699 1.78.872 2.607.33a18.095 18.095 0 005.223-5.223c.542-.827.369-1.908-.33-2.607L11.16 3.66A2.25 2.25 0 009.568 3z" />
              <path strokeLinecap="round" strokeLinejoin="round" d="M6 6h.008v.008H6V6z" />
            </svg>
          </div>
          <p className="text-archive-500 text-sm mb-1">No categories yet</p>
          <p className="text-archive-600 text-xs">Create one to organize your archived items</p>
        </div>
      ) : (
        <div className="space-y-2">
          {categories.map((cat) => (
            <div key={cat.id} className="card p-4 flex items-center justify-between group hover:border-archive-700 transition-colors">
              <div className="flex items-center gap-3">
                <div className="w-3 h-3 rounded-full flex-shrink-0" style={{ backgroundColor: cat.color }} />
                <div>
                  <p className="text-sm font-medium text-archive-200">{cat.name}</p>
                  {cat.description && (
                    <p className="text-xs text-archive-500 mt-0.5">{cat.description}</p>
                  )}
                </div>
              </div>
              <div className="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                <button onClick={() => handleEdit(cat)} className="btn-ghost p-1.5 text-archive-500 hover:text-archive-200">
                  <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M16.862 4.487l1.687-1.688a1.875 1.875 0 112.652 2.652L10.582 16.07a4.5 4.5 0 01-1.897 1.13L6 18l.8-2.685a4.5 4.5 0 011.13-1.897l8.932-8.931zm0 0L19.5 7.125" />
                  </svg>
                </button>
                <button onClick={() => handleDelete(cat.id)} className="btn-ghost p-1.5 text-archive-500 hover:text-red-400">
                  <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M14.74 9l-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 01-2.244 2.077H8.084a2.25 2.25 0 01-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 00-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 013.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 00-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 00-7.5 0" />
                  </svg>
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
