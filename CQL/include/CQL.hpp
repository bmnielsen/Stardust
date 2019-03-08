#pragma once

#include "CQL/Serialize.hpp"
#include "CQL/Custom.hpp"

#include <unordered_set>
#include <type_traits>
#include <algorithm>
#include <utility>
#include <memory>
#include <tuple>
#include <set>

namespace CQL {
  template<typename T, bool Instantiate>
  struct ConditionalVar { T val; };

  template<typename T>
  struct ConditionalVar<T, false> { };

  template<typename T> // This exists in C++20, use std::remove_cvref ASAP.
  struct remove_cvref { typedef std::remove_cv<std::remove_reference_t<T>> type; };

  template<typename T>
  using remove_cvref_v = typename remove_cvref<T>::type;

  template<typename Entry>
  struct Table {
    Table() = default;

  private:
    struct AndOperation; struct OrOperation;
  public:
#define ExprOperators                                                  \
    template<typename Expr>                                            \
    auto operator&&(Expr &&other) && {                                 \
      return makeExpr<AndOperation>(std::move(*this),                  \
                                    std::forward<Expr>(other));        \
    }                                                                  \
                                                                       \
    template<typename Expr>                                            \
    auto operator&&(Expr &&other) & {                                  \
      return makeExpr<AndOperation>(*this, std::forward<Expr>(other)); \
    }                                                                  \
                                                                       \
    template<typename Expr>                                            \
    auto operator||(Expr &&other) && {                                 \
      return makeExpr<OrOperation>(std::move(*this),                   \
                                   std::forward<Expr>(other));         \
    }                                                                  \
                                                                       \
    template<typename Expr>                                            \
    auto operator||(Expr &&other) & {                                  \
      return makeExpr<OrOperation>(*this, std::forward<Expr>(other));  \
    }

#define ForEachOperator                                                \
    template<typename F>                                               \
    auto operator>>=(F functor) { return forEach(functor); }

    template<typename F>
    struct Predicate {
      Predicate(F &&f) : f{ f } { }
      bool operator()(Entry const &entry) { return f(entry); }

      ExprOperators

    private:
      F f;
    };

    template<typename RangeL, typename RangeR>
    struct RangeUnion {
      RangeUnion(RangeL &&rl, RangeR &&rr): rl{ std::forward<RangeL>(rl) }, rr{ std::forward<RangeR>(rr) } { }

      template<typename F>
      void forEach(F functor) {
        rl.forEach([&](Entry const &entry) {
          functor(entry);
        });

        rr.forEach([&](Entry const &entry) {
          if (!rl(entry)) {
            functor(entry);
          }
        });
      }

      bool operator()(Entry const &other) const {
        return rl(other) || rr(other);
      }

      ExprOperators
      ForEachOperator

    private:
      RangeL rl;
      RangeR rr;
    };

    template<typename Range, typename Pred>
    struct FilteredRangeExpr {
      FilteredRangeExpr(Range &&range, Pred &&predicate) : range{ range }, predicate{ predicate } { }

      bool operator()(Entry const &entry) {
        return range(entry) && predicate(entry);
      }

      template<typename F>
      void forEach(F functor) {
        range.forEach([&](Entry const &entry) {
          if (predicate(entry)) {
            functor(entry);
          }
        });
      }

      ExprOperators
      ForEachOperator

    private:
      Range range;
      Pred predicate;
    };

    template<size_t Ind, typename It>
    struct Iterator {
      Iterator &operator++() {
        ++setIt;
        return *this;
      }

      Iterator &operator--() {
        --setIt;
        return *this;
      }

      Entry const &operator*() const {
        return **setIt;
      }

      bool operator==(Iterator<Ind, It> const &other) const {
        return setIt == other.setIt;
      }

      bool operator!=(Iterator<Ind, It> const &other) const {
        return setIt != other.setIt;
      }

      Entry const *ptr() const {
        return *setIt;
      }

    protected:
      friend struct Table;

      template<size_t _Ind, typename _It>
      friend struct Range;

      explicit Iterator(It &&it) : setIt(std::forward<It>(it)) { }

      It setIt;
    };

    template<size_t Ind, typename Lt, typename Ht, typename Tt>
    struct Range {
      Range(Lt &&lo, Ht &&hi, Tt const &tbl):
        lo{ std::forward<Lt>(lo) }, hi{ std::forward<Ht>(hi) }, tbl{tbl} { }

      template<typename F>
      void forEach(F functor) const {
        for (auto it = tbl.lower_bound(lo); it != tbl.upper_bound(hi); ++it) {
          functor(**it);
        }
      }

      bool operator()(Entry const &other) const {
        return lo <= std::get<Ind>(other)
                  && std::get<Ind>(other) <= hi;
      }

      ExprOperators
      ForEachOperator

    private:
      std::tuple_element_t<Ind, Entry> const lo, hi;
      Tt &tbl;
    };

    template<size_t Ind, typename Tt>
    struct EntireTable {
      EntireTable(Tt const &tbl):
        tbl{tbl} { }

      template<typename F>
      void forEach(F functor) const {
        for (auto &v : tbl) {
          functor(*v);
        }
      }

      bool operator()(Entry const &other) const {
        return true;
      }

      ExprOperators
      ForEachOperator

    private:
      Tt const &tbl;
    };

#undef ForEachOperator
#undef ExprOperators

    Entry const *emplace(std::unique_ptr<Entry> &&e) {
      if (shouldInsert<0>(e)) {
        auto ret = e.get();
        if constexpr(Custom::DefaultLookup<Entry>{}() == std::tuple_size_v<Entry>) {
          updateAll<0>(e.get());
          defaultLUT.val.emplace(std::forward<decltype(e)>(e));
        }
        else {
          updateAll<0>(std::forward<decltype(e)>(e));
        }
        return ret;
      }
      return nullptr;
    }

    template<typename ...Args>
    Entry const *emplace(Args &&...args) {
      auto ptr = std::make_unique<Entry>(std::forward<Args>(args)...);
      return emplace(std::move(ptr));
    }

    void erase(Entry const *entry) {
      eraseAll<0>(entry);
      if constexpr(Custom::DefaultLookup<Entry>{}() == std::tuple_size_v<Entry>) {
        defaultLUT.val.erase(defaultLUT.val.find(entry));
      }
    }

    auto begin() const {
      auto it = defaultLookup().begin();
      return Iterator<std::tuple_size<Entry>::value, decltype(it)>(std::move(it));
    }

    auto end() const {
      auto it = defaultLookup().end();
      return Iterator<std::tuple_size<Entry>::value, decltype(it)>(std::move(it));
    }

    template<size_t Ind>
    auto vbegin() const {
      auto it = std::get<Ind>(luts).begin();
      return Iterator<Ind, decltype(it)>(std::move(it));
    }

    template<size_t Ind>
    auto rvbegin() const {
      auto it = std::get<Ind>(luts).rbegin();
      return Iterator<Ind, decltype(it)>(std::move(it));
    }

    template<size_t Ind>
    auto vend() const {
      auto it = std::get<Ind>(luts).end();
      return Iterator<Ind, decltype(it)>(std::move(it));
    }

    template<size_t Ind>
    auto rvend() const {
      auto it = std::get<Ind>(luts).rend();
      return Iterator<Ind, decltype(it)>(std::move(it));
    }

    template<size_t Ind, typename T>
    auto erase(Iterator<Ind, T> &it) {
      if constexpr(Ind == std::tuple_size<Entry>::value) {
        eraseAll<0>(*(it.setIt));
        return Iterator<Ind, T>(defaultLUT.val.erase(it.setIt));
      }
      else {
        if constexpr(Custom::DefaultLookup<Entry>{}() == std::tuple_size_v<Entry>) {
          defaultLUT.val.erase(*(it.setIt));
        }
        return eraseIt<std::tuple_size<Entry>::value, Ind>(it);
      }
    }

    size_t size() const {
      return defaultLookup().size();
    }

    bool empty() const {
      return defaultLookup().empty();
    }

    void clear() {
      clearAll<0>();
      if constexpr(Custom::DefaultLookup<Entry>{}() == std::tuple_size_v<Entry>) {
        defaultLUT.val.clear();
      }
    }

    std::unique_ptr<Entry> extract(Entry const *entry) {
      auto ptr = std::move(defaultLookup().extract(defaultLookup().find(entry)).value());
      eraseAll<0>(ptr.get());
      return ptr;
    }

    template<size_t N, typename T>
    Entry const *lookup(T const &val) {
      Entry const *ret = nullptr;
      if (auto it = std::get<N>(luts).find(val); it != std::get<N>(luts).end()) {
        if constexpr(CQL::Custom::DefaultLookup<Entry>{}() == N) {
          ret = it->get();
        }
        else {
          ret = *it;
        }
      }

      return ret;
    }

    // Returns false if the value already exists in a table where enforced uniqueness exists
    template<size_t N, typename T>
    bool update(Entry const *entry, T &&newVal) {
      if constexpr(Custom::Unique<Entry, N>{}() == Custom::Uniqueness::EnforceUnique) {
        if (auto f = std::get<N>(luts).find(newVal); f != std::get<N>(luts).end() && &**f != entry) {
          return false;
        }
        else if (&**f == entry) {
          return true;
        }
      }

      auto dbEntry = std::move(moveOutOfTable<N>(entry).value());
      std::get<N>(*dbEntry) = std::forward<T>(newVal);
      std::get<N>(luts).emplace(std::move(dbEntry));
      return true;
    }

    template<size_t N, size_t OtherN, typename T, typename ItT>
    bool update(Iterator<OtherN, ItT> const &entry, T &&newVal) {
      if constexpr(N == OtherN) {
        throw std::runtime_error {
          "You cannot update the value of an object when you are iterating over the value you want to change"
        };
      }
      else {
        return update(*(entry.setIt));
      }
    }

    // Returns false if the value already exists in a table where enforced uniqueness exists
    template<size_t N, typename T>
    bool swap(Entry const *entry, T &newVal) {
      if constexpr(Custom::Unique<Entry, N>{}() == Custom::Uniqueness::EnforceUnique) {
        if (std::get<N>(luts).find(newVal) != std::get<N>(luts).end())
          return false;
      }

      auto dbEntry = std::move(moveOutOfTable<N>(entry).value());
      std::swap(std::get<N>(*dbEntry), newVal);
      std::get<N>(luts).emplace(std::move(dbEntry));
      return true;
    }

    template<size_t N, typename T1, typename T2>
    auto range(T1 &&lb, T2 &&ub) {
      return makeRange<N>(std::forward<T1>(lb),
                          std::forward<T2>(ub));
    }

    auto all() const {
      auto &tbl = defaultLookup();
      return EntireTable<Custom::DefaultLookup<Entry>{}(), decltype(tbl)>{tbl};
    }

    template<typename F>
    static auto pred(F &&predicate) {
      return Predicate<F>{std::forward<F>(predicate)};
    }

  private:
    template<typename Operator, typename LE, typename RE>
    struct makeExprImpl {
      auto operator()(LE &&le, RE &&re) {
        if constexpr(std::is_same_v<Operator, AndOperation>) {
          return makeExpr<AndOperation>(std::forward<LE>(le), pred([r = std::forward<RE>(re)](auto val) { return r(val); }));
        }
        else {
          return RangeUnion<LE, RE>{ std::forward<LE>(le), std::forward<RE>(re) };
        }
      }
    };

    template<typename LE, typename T>
    struct makeExprImpl<AndOperation, LE, Predicate<T>> {
      auto operator()(LE &&expr, Predicate<T> &&pred) {
        return FilteredRangeExpr<LE, Predicate<T>>{ std::forward<LE>(expr), std::forward<Predicate<T>>(pred) };
      }
    };

    template<typename Operator, typename LE, typename RE>
    static auto makeExpr(LE &&le, RE &&re) {
      return makeExprImpl<Operator, LE, RE>{}(std::forward<LE>(le), std::forward<RE>(re));
    }

    template<size_t N, typename Lt, typename Ht>
    auto makeRange(Lt &&itl, Ht &&itr) {
      return Range<N, Lt, Ht, decltype(std::get<N>(std::declval<decltype(luts)>()))>
        { std::forward<Lt>(itl), std::forward<Ht>(itr), std::get<N>(luts) };
    }

    template<size_t N>
    struct Compare {
      template<typename T1, typename T2>
      bool operator()(T1 const &lhs, T2 const &rhs) const {
        auto val = [](auto const &v) {
          if constexpr(N == std::tuple_size_v<Entry>) {
            if constexpr(std::is_same<decltype(v), std::unique_ptr<Entry> const &>::value) {
              return v.get();
            }
            else {
              return v;
            }
          }
          else if constexpr(std::is_same<decltype(v), Entry *const &>::value
                         || std::is_same<decltype(v), Entry const *const &>::value
                         || std::is_same<decltype(v), std::unique_ptr<Entry> const &>::value) {
            return std::get<N>(*v);
          }
          else {
            return v;
          }
        };

        return val(lhs) < val(rhs);
      }

      using is_transparent = void;
    };

    template<size_t Idx>
    static decltype(auto) makeSet() {
      if constexpr(Custom::DefaultLookup<Entry>{}() == Idx) {
        return std::set<std::unique_ptr<Entry>, Compare<Idx>>{};
      }
      else if constexpr(Custom::Unique<Entry, Idx>{}() == Custom::Uniqueness::NotUnique) {
        return std::multiset<Entry *, Compare<Idx>>{};
      }
      else {
        return std::set<Entry *, Compare<Idx>>{};
      }
    }

    template<size_t ...Is>
    static decltype(auto) makeSets(std::index_sequence<Is...>) {
      return std::make_tuple(makeSet<Is>()...);
    }

    using Sets = decltype(makeSets(std::make_index_sequence<std::tuple_size<Entry>::value>{}));

    Sets luts{};

    ConditionalVar<std::set<std::unique_ptr<Entry>, Compare<std::tuple_size<Entry>::value>>,
                   std::tuple_size_v<Entry> == CQL::Custom::DefaultLookup<Entry>{}()> defaultLUT;

    auto constexpr &defaultLookup() const {
      if constexpr(Custom::DefaultLookup<Entry>{}() != std::tuple_size_v<Entry>) {
        static_assert(Custom::Unique<Entry, Custom::DefaultLookup<Entry>{}()>{}()
                                         != Custom::Uniqueness::NotUnique);
        return std::get<Custom::DefaultLookup<Entry>{}()>(luts);
      }
      else {
        return defaultLUT.val;
      }
    }

    auto constexpr &defaultLookup() {
      if constexpr(Custom::DefaultLookup<Entry>{}() != std::tuple_size_v<Entry>) {
        static_assert(Custom::Unique<Entry, Custom::DefaultLookup<Entry>{}()>{}()
                                         != Custom::Uniqueness::NotUnique);
        return std::get<Custom::DefaultLookup<Entry>{}()>(luts);
      }
      else {
        return defaultLUT.val;
      }
    }

    template<size_t N, typename T>
    void updateAll(T &&entry) {
      if constexpr(N < std::tuple_size<Entry>::value) {
        if constexpr(N == Custom::DefaultLookup<Entry>{}()) {
          updateAll<N + 1>(entry.get());
          std::get<N>(luts).emplace(std::forward<T>(entry));
        }
        else if constexpr(std::is_same<T, std::unique_ptr<Entry>>::value) {
          std::get<N>(luts).emplace(entry.get());
          updateAll<N + 1>(std::forward<T>(entry));
        }
        else {
          std::get<N>(luts).emplace(entry);
          updateAll<N + 1>(entry);
        }
      }
    }

    template<size_t N, typename T>
    void eraseAll(T const &entry) {
      if constexpr(N < std::tuple_size<Entry>::value) {
        if (auto it = findInLut<N>(entry); it != std::get<N>(luts).end()) {
          std::get<N>(luts).erase(it);
        }
        eraseAll<N + 1>(entry);
      }
    }

    template<size_t N>
    void clearAll() {
      if constexpr(N < std::tuple_size<Entry>::value) {
        std::get<N>(luts).clear();
        clearAll<N + 1>();
      }
    }

    template<size_t N, size_t Er, typename T>
    auto eraseIt(T const &it) {
      if constexpr(N < std::tuple_size<Entry>::value) {
        if constexpr(N == Er) {
          eraseIt<N + 1, Er>(it);
          return std::get<N>(luts).erase(it);
        }
        else {
          std::get<N>(luts).erase(findInLut<N>(*(it.setIt)));
          return eraseIt<N + 1, Er>(it);
        }
      }
    }

    template<size_t N, typename T>
    auto findInLut(T const &val) const {
      if constexpr(Custom::Unique<Entry, N>{}() == Custom::Uniqueness::NotUnique) {
        auto[low, hi] = std::get<N>(luts).equal_range(val);
        if (auto f = std::find_if(low, hi, [&](auto v) {
          return &*v == &*val;
        }); f != hi)
          return f;
      }
      else {
          return std::get<N>(luts).find(val);
      }
      return std::get<N>(luts).end();
    }

    template<size_t N, typename T>
    auto moveOutOfTable(T const &val) {
      return std::get<N>(luts).extract(findInLut<N>(val));
    }
    
    template<size_t N, typename T>
    auto shouldInsert(T const &val) const {
      if constexpr(N < std::tuple_size<Entry>::value) {
        if constexpr(Custom::Unique<Entry, N>{}() == Custom::Uniqueness::EnforceUnique) {
          return (std::get<N>(luts).find(val) == std::get<N>(luts).end()) && shouldInsert<N + 1>(val);
        }
        else {
          return shouldInsert<N + 1>(val);
        }
      }
      else {
        return true;
      }
    }
  };

  template<typename T>
  struct Detail::Serialize<Table<T>> {
    void operator()(std::ostream &os, Table<T> const &table) const {
      uint64_t const s = table.size();
      serialize(os, s);
      for (auto &entry : table) {
        serialize(os, entry);
      }
    }
  };

  template<typename T>
  struct Detail::Deserialize<Table<T>> {
    Table<T> operator()(std::istream &is) const {
      Table<T> table;
      auto const s = static_cast<size_t>(deserialize<uint64_t>(is));

      for(size_t i = 0; i < s; ++ i) {
        table.emplace(deserialize<T>(is));
      }

      return table;
    }
  };
}

