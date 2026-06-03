#pragma once

#include <cstdint>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// ECS (Entity Component System) de MoteurJV.
//
//   Entity    = un simple identifiant (un entier).
//   Component = des données pures (Transform2D, Velocity, ...). Aucune logique.
//   System    = la logique, écrite à l'extérieur : on parcourt les entités qui
//               possèdent un certain jeu de composants via Registry::view<...>().
//
// Stockage "data-oriented" : les composants d'un même type sont rangés de façon
// contiguë (sparse set), donc le parcours par les systèmes est cache-friendly.
// -----------------------------------------------------------------------------

namespace mjv {

using Entity = std::uint32_t;
inline constexpr Entity NullEntity = 0xFFFFFFFFu;

namespace detail {

// Interface commune pour stocker tous les pools dans une même table.
class IPool {
public:
    virtual ~IPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
};

// Pool d'un type de composant T : tableaux denses + table entité -> index.
template <class T>
class Pool : public IPool {
public:
    T& add(Entity e, T comp) {
        auto it = m_index.find(e);
        if (it != m_index.end()) {            // déjà présent : on remplace
            m_dense[it->second] = std::move(comp);
            return m_dense[it->second];
        }
        m_index[e] = m_dense.size();
        m_dense.push_back(std::move(comp));
        m_entities.push_back(e);
        return m_dense.back();
    }

    bool has(Entity e) const override { return m_index.find(e) != m_index.end(); }

    T& get(Entity e) { return m_dense[m_index.at(e)]; }

    void remove(Entity e) override {
        auto it = m_index.find(e);
        if (it == m_index.end()) return;
        const std::size_t idx = it->second;
        const std::size_t last = m_dense.size() - 1;
        // swap-and-pop pour garder le tableau dense compact.
        m_dense[idx] = std::move(m_dense[last]);
        m_entities[idx] = m_entities[last];
        m_index[m_entities[idx]] = idx;
        m_dense.pop_back();
        m_entities.pop_back();
        m_index.erase(it);
    }

    std::vector<Entity>& entities() { return m_entities; }
    std::vector<T>& data() { return m_dense; }

private:
    std::vector<T> m_dense;                          // les composants, contigus
    std::vector<Entity> m_entities;                  // entité de chaque slot dense
    std::unordered_map<Entity, std::size_t> m_index; // entité -> index dense
};

} // namespace detail

class Registry {
public:
    // --- Cycle de vie des entités ----------------------------------------
    Entity create() {
        Entity e;
        if (!m_free.empty()) {
            e = m_free.back();
            m_free.pop_back();
        } else {
            e = m_next++;
        }
        m_alive.insert(e);
        return e;
    }

    void destroy(Entity e) {
        for (auto& [type, pool] : m_pools) {
            pool->remove(e);
        }
        m_alive.erase(e);
        m_free.push_back(e);
    }

    bool valid(Entity e) const { return m_alive.find(e) != m_alive.end(); }

    // --- Composants -------------------------------------------------------
    template <class T>
    T& add(Entity e, T comp = T{}) {
        return pool<T>().add(e, std::move(comp));
    }

    template <class T>
    bool has(Entity e) {
        auto it = m_pools.find(std::type_index(typeid(T)));
        if (it == m_pools.end()) return false;
        return static_cast<detail::Pool<T>*>(it->second.get())->has(e);
    }

    template <class T>
    T& get(Entity e) {
        return pool<T>().get(e);
    }

    template <class T>
    void remove(Entity e) {
        pool<T>().remove(e);
    }

    // --- Parcours : la base de tout System --------------------------------
    // Appelle func(Entity, T&, Rest&...) pour chaque entité possédant T et tous
    // les Rest. Exemple : reg.view<Transform2D, Velocity>([](Entity, auto& t, auto& v){...});
    template <class T, class... Rest, class Func>
    void view(Func func) {
        detail::Pool<T>& p = pool<T>();
        auto& ents = p.entities();
        auto& comps = p.data();
        for (std::size_t i = 0; i < ents.size(); ++i) {
            const Entity e = ents[i];
            if ((has<Rest>(e) && ...)) {
                func(e, comps[i], get<Rest>(e)...);
            }
        }
    }

private:
    template <class T>
    detail::Pool<T>& pool() {
        const std::type_index ti(typeid(T));
        auto it = m_pools.find(ti);
        if (it == m_pools.end()) {
            auto created = std::make_unique<detail::Pool<T>>();
            detail::Pool<T>* raw = created.get();
            m_pools.emplace(ti, std::move(created));
            return *raw;
        }
        return *static_cast<detail::Pool<T>*>(it->second.get());
    }

    Entity m_next = 0;
    std::vector<Entity> m_free;
    std::unordered_set<Entity> m_alive;
    std::unordered_map<std::type_index, std::unique_ptr<detail::IPool>> m_pools;
};

} // namespace mjv
