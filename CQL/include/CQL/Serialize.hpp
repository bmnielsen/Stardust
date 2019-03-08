#pragma once

#include <iostream>
#include <iterator>
#include <utility>
#include <tuple>

namespace CQL {
  namespace Detail {
    template<typename T>
    struct removeConstConditional_ { using type = T; };

    template<typename T>
    struct removeConstConditional_<T const> { using type = T; };

    template<typename T>
    using removeConstConditional = typename removeConstConditional_<T>::type;

    template<typename T>
    struct tupleRemoveConst_ { using type = T; };

    template<typename ...Ts>
    struct tupleRemoveConst_<std::tuple<Ts...>> {
      using type = std::tuple<removeConstConditional<Ts >...>;
    };

    template<typename ...Ts>
    struct tupleRemoveConst_<std::pair<Ts...>> {
      using type = std::pair<removeConstConditional<Ts>...>;
    };

    template<typename T, size_t s>
    struct tupleRemoveConst_<std::array<T, s>> {
      using type = std::array<removeConstConditional<T>, s>;
    };

    template <typename T>
    using tupleRemoveConst = removeConstConditional<typename tupleRemoveConst_<T>::type>;

    template<typename T> struct isTuploid_ : std::false_type { };
    template<typename ...Ts> struct isTuploid_<std::tuple<Ts...>> : std::true_type { };
    template<typename ...Ts> struct isTuploid_<std::pair<Ts...>> : std::true_type { };
    template<typename T, size_t s> struct isTuploid_<std::array<T, s>> : std::true_type { };

    template<typename T>
    constexpr bool isTuploid = isTuploid_<T>();

    template<typename T, typename = void> struct isContainer_ : std::false_type { };
    template<typename T> struct isContainer_ <T, std::void_t<
      decltype(std::begin(std::declval<T>()) == std::end(std::declval<T>())),
      decltype(std::next(std::begin(std::declval<T>()))),
      decltype(*std::begin(std::declval<T>())),
      decltype(std::size(std::declval<T>()))
      >> : std::true_type { };

    template<typename T>
    constexpr bool isContainer = isContainer_<T>();

    template<typename T, typename = void> struct isContinuous_ : std::false_type { };
    template<typename T> struct isContinuous_ <T, std::void_t<
      decltype(*std::declval<T>().data()),
      decltype(std::declval<T>().size())
      >> : std::true_type { };

    template<typename T>
    constexpr bool isContinuous = isContainer<T> && isContinuous_<T>();

    template<typename T, typename = void>
    struct Serialize;
  }

  template<typename T>
  void serialize(std::ostream &os, T const &val) {
    Detail::Serialize<T>{}(os, val);
  }

  namespace Detail {
    template<size_t idx, typename Tuple_t>
    void serializeTuploid(std::ostream &os, Tuple_t const &t) {
      if constexpr (idx < std::tuple_size_v<Tuple_t>) {
        serialize(os, std::get<idx>(t));
        serializeTuploid<idx + 1, Tuple_t>(os, t);
      }
    }

    template<typename T, typename>
    struct Serialize {
      void operator()(std::ostream &os, T const &val) const {
        if constexpr(std::is_integral_v<T>) {
          os.write(reinterpret_cast<const char *>(&val), sizeof(T));
        }
        else if constexpr(isTuploid<T>) {
          serializeTuploid<0>(os, val);
        }
        else if constexpr(isContainer<T>) {
          uint64_t const s = val.size();
          serialize(os, s);
          for (auto it = val.begin(); it != val.end(); ++it) {
            serialize(os, *it);
          }
        }
        else {
          val.serialize(os);
        }
      }
    };

    template<typename T>
    struct Serialize<T, std::enable_if_t<
        isContinuous<T>
     && std::is_trivial_v<typename T::value_type>
     && std::is_standard_layout_v<typename T::value_type>
        >> {
      void operator()(std::ostream &os, T const &val) { // Vector<int>, string etc optimization
        uint64_t const s = val.size();
        serialize(os, s);
        os.write(reinterpret_cast<const char *>(val.data()), s * sizeof(typename T::value_type));
      }
    };

    template<typename T>
    struct Deserialize;
  }

  template<typename T>
  T deserialize(std::istream &is) {
    return Detail::Deserialize<T>{}(is);
  }

  namespace Detail {
    template<typename Tuple_t, size_t idx>
    void deserializeTuploid(std::istream &is, Tuple_t &t) {
      if constexpr (idx < std::tuple_size_v<Tuple_t>) {
        std::get<idx>(t) = deserialize<std::tuple_element_t<idx, Tuple_t>>(is);
        deserializeTuploid<Tuple_t, idx + 1>(is, t);
      }
    }

    template<typename T>
    struct Deserialize {
      T operator()(std::istream &is) const {
        if constexpr(std::is_integral_v<T>) {
          T temp;
          is.read(reinterpret_cast<char *>(&temp), sizeof(T));
          return temp;
        }
        else if constexpr(isTuploid<T>) {
          tupleRemoveConst<T> temp;
          deserializeTuploid<tupleRemoveConst<T>, 0>(is, temp);
          return temp;
        }
        else if constexpr(isContainer<T>) {
          auto s = static_cast<size_t>(deserialize<uint64_t>(is));
          T container;
          for (auto it = std::insert_iterator<T>{ container, std::begin(container) }; s --> 0;) {
            it = deserialize<tupleRemoveConst<typename T::value_type>>(is);
          }
          return container;
        }
        else {
          return T::deserialize(is);
        }
      }
    };
  }
}
