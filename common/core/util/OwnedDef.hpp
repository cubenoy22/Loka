// OwnedDef.hpp - C++98 ownership wrapper for cloned definitions.
// Loka Project util (core/util/OwnedDef.hpp)

#ifndef LOKA_CORE_UTIL_OWNEDDEF_HPP
#define LOKA_CORE_UTIL_OWNEDDEF_HPP

namespace loka
{
  namespace core
  {
    /** Owns one nullable, exclusively owned definition value. */
    template <typename T> class OwnedDef
    {
      T *ptr_;

    public:
      explicit OwnedDef(T *p = 0)
          : ptr_(p)
      {
      }
      ~OwnedDef()
      {
        delete ptr_;
      }
      T *get() const
      {
        return ptr_;
      }
      T *operator->() const
      {
        return ptr_;
      }
      T &operator*() const
      {
        return *ptr_;
      }
      bool isSet() const
      {
        return ptr_ != 0;
      }
      void reset(T *p = 0)
      {
        if (ptr_ != p)
        {
          delete ptr_;
          ptr_ = p;
        }
      }
      T *take()
      {
        T *value = ptr_;
        ptr_ = 0;
        return value;
      }

    private:
      OwnedDef(const OwnedDef &);
      OwnedDef &operator=(const OwnedDef &);
    };
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_UTIL_OWNEDDEF_HPP
