import { Routes, Route } from "react-router-dom";
import Layout from "./components/Layout";
import Dashboard from "./pages/Dashboard";
import ItemsList from "./pages/ItemsList";
import ItemDetail from "./pages/ItemDetail";
import Categories from "./pages/Categories";
import Search from "./pages/Search";
import Favorites from "./pages/Favorites";
import Trash from "./pages/Trash";

export default function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<Dashboard />} />
        <Route path="/items" element={<ItemsList />} />
        <Route path="/items/:id" element={<ItemDetail />} />
        <Route path="/categories" element={<Categories />} />
        <Route path="/search" element={<Search />} />
        <Route path="/favorites" element={<Favorites />} />
        <Route path="/trash" element={<Trash />} />
      </Route>
    </Routes>
  );
}
