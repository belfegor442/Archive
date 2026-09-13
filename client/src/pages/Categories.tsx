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

  useEffect(() => {
    api.categories
      .list()
      .then(setCategories)
      .catch(console.error)
      .finally(() => setLoading(false));
  }, []);

  const handleCreate = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim()) return;
    try {
      const cat = await api.categories.create({ name, description, color });
      setCategories((prev) => [...prev, cat]);
      setName("");
      setDescription("");
      setShowForm(false);
    } catch (err) {
      console.error(err);
    }
  };

  const handleDelete = async (id: string) => {
    try {
      await api.categories.delete(id);
      setCategories((prev) => prev.filter((c) => c.id !== id));
    } catch (err) {
      console.error(err);
    }
  };

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-archive-100">Categories</h1>
        <button onClick={() => setShowForm(!showForm)} className="btn-primary">
          {showForm ? "Cancel" : "New Category"}
        </button>
      </div>

      {showForm && (
        <form
          onSubmit={handleCreate}
          className="card p-4 mb-6 space-y-3 max-w-md"
        >
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            className="input"
            placeholder="Category name"
          />
          <input
            type="text"
            value={description}
            onChange={(e) => setDescription(e.target.value)}
            className="input"
            placeholder="Description (optional)"
          />
          <div className="flex items-center gap-2">
            <label className="text-xs text-archive-500">Color</label>
            <input
              type="color"
              value={color}
              onChange={(e) => setColor(e.target.value)}
              className="w-8 h-8 rounded border border-archive-700 bg-transparent cursor-pointer"
            />
          </div>
          <button type="submit" className="btn-primary">
            Create
          </button>
        </form>
      )}

      {loading ? (
        <p className="text-archive-500 text-sm">Loading...</p>
      ) : categories.length === 0 ? (
        <p className="text-archive-500 text-sm">
          No categories yet. Create one to organize your archived items.
        </p>
      ) : (
        <div className="space-y-2">
          {categories.map((cat) => (
            <div
              key={cat.id}
              className="card p-3 flex items-center justify-between"
            >
              <div className="flex items-center gap-3">
                <div
                  className="w-3 h-3 rounded-full"
                  style={{ backgroundColor: cat.color }}
                />
                <div>
                  <p className="text-sm font-medium text-archive-200">
                    {cat.name}
                  </p>
                  {cat.description && (
                    <p className="text-xs text-archive-500">
                      {cat.description}
                    </p>
                  )}
                </div>
              </div>
              <button
                onClick={() => handleDelete(cat.id)}
                className="btn-ghost text-archive-600 hover:text-red-400 text-xs"
              >
                Delete
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
