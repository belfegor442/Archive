import { useState, useEffect, useRef } from "react";
import { api } from "../api";
import ItemCard from "../components/ItemCard";
import type { ArchiveItem, Category } from "../types";

export default function Search() {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<ArchiveItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);
  const [typeFilter, setTypeFilter] = useState("");
  const [categoryFilter, setCategoryFilter] = useState("");
  const [categories, setCategories] = useState<Category[]>([]);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    api.categories.list().then(setCategories).catch(console.error);
    inputRef.current?.focus();
  }, []);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === "k") {
        e.preventDefault();
        inputRef.current?.focus();
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, []);

  const handleSearch = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!query.trim()) return;

    setLoading(true);
    setSearched(true);
    try {
      const params: Record<string, string> = {};
      if (typeFilter) params.type = typeFilter;
      if (categoryFilter) params.categoryId = categoryFilter;
      const data = await api.search(query, Object.keys(params).length > 0 ? params : undefined);
      setResults(data.items);
    } catch (err) {
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleClear = () => {
    setQuery("");
    setResults([]);
    setSearched(false);
    setTypeFilter("");
    setCategoryFilter("");
    inputRef.current?.focus();
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold text-archive-100 mb-6">Search</h1>

      <form onSubmit={handleSearch} className="mb-6">
        <div className="flex gap-2 mb-3">
          <div className="relative flex-1">
            <svg className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-archive-500" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M21 21l-5.197-5.197m0 0A7.5 7.5 0 105.196 5.196a7.5 7.5 0 0010.607 10.607z" />
            </svg>
            <input
              ref={inputRef}
              type="text"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              className="input pl-10 pr-20"
              placeholder="Search by name, tags, description..."
            />
            <span className="absolute right-3 top-1/2 -translate-y-1/2 text-xs text-archive-600 bg-archive-800 px-1.5 py-0.5 rounded">
              Ctrl+K
            </span>
          </div>
          <button type="submit" className="btn-primary" disabled={loading}>
            {loading ? "Searching..." : "Search"}
          </button>
          {searched && (
            <button type="button" onClick={handleClear} className="btn-ghost">
              Clear
            </button>
          )}
        </div>

        <div className="flex gap-2">
          <select
            value={typeFilter}
            onChange={(e) => setTypeFilter(e.target.value)}
            className="input w-auto text-xs"
          >
            <option value="">All types</option>
            <option value="project">Projects</option>
            <option value="file">Files</option>
            <option value="folder">Folders</option>
            <option value="document">Documents</option>
          </select>
          <select
            value={categoryFilter}
            onChange={(e) => setCategoryFilter(e.target.value)}
            className="input w-auto text-xs"
          >
            <option value="">All categories</option>
            {categories.map((cat) => (
              <option key={cat.id} value={cat.id}>{cat.name}</option>
            ))}
          </select>
        </div>
      </form>

      {loading ? (
        <div className="flex items-center gap-2 text-archive-500 py-8">
          <svg className="animate-spin h-4 w-4" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" fill="none" />
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
          </svg>
          <span className="text-sm">Searching...</span>
        </div>
      ) : searched && results.length === 0 ? (
        <div className="text-center py-12">
          <p className="text-archive-500 text-sm">No results found for "{query}"</p>
          <p className="text-archive-600 text-xs mt-1">Try different keywords or adjust filters</p>
        </div>
      ) : results.length > 0 ? (
        <div>
          <p className="text-xs text-archive-500 mb-3">{results.length} result(s) found</p>
          <div className="grid grid-cols-3 gap-3">
            {results.map((item) => (
              <ItemCard key={item.id} item={item} />
            ))}
          </div>
        </div>
      ) : !searched ? (
        <div className="text-center py-12">
          <p className="text-archive-600 text-sm">Type a query and press Enter to search</p>
        </div>
      ) : null}
    </div>
  );
}
