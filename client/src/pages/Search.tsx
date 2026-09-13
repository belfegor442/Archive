import { useState } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem } from "../types";

export default function Search() {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);
  const [typeFilter, setTypeFilter] = useState("");

  const handleSearch = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!query.trim()) return;

    setLoading(true);
    setSearched(true);
    try {
      const data = await api.search(query, typeFilter ? { type: typeFilter } : undefined);
      setResults(data.items);
    } catch (err) {
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Search</h1>

      <form onSubmit={handleSearch} className="flex gap-2 mb-6">
        <input
          type="text"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          className="input flex-1"
          placeholder="Search by name, tags, description..."
        />
        <select
          value={typeFilter}
          onChange={(e) => setTypeFilter(e.target.value)}
          className="input w-auto"
        >
          <option value="">All types</option>
          <option value="project">Projects</option>
          <option value="file">Files</option>
          <option value="folder">Folders</option>
          <option value="document">Documents</option>
        </select>
        <button type="submit" className="btn-primary">
          Search
        </button>
      </form>

      {loading ? (
        <p className="text-archive-500 text-sm">Searching...</p>
      ) : searched && results.length === 0 ? (
        <p className="text-archive-500 text-sm">No results found.</p>
      ) : (
        <div className="grid grid-cols-3 gap-3">
          {results.map((item) => (
            <ItemCard key={item.id} item={item} />
          ))}
        </div>
      )}
    </div>
  );
}
