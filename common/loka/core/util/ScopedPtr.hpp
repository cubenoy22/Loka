// ScopedPtr.hpp - C++98用シンプルRAIIスマートポインタ
// Loka Project util (loka/core/util/ScopedPtr.hpp)
//
// - コピー禁止
// - スコープ終了時にdelete
// - NSAutoreleasePool的な使い方も可

#ifndef LOKA_CORE_UTIL_SCOPEDPTR_HPP
#define LOKA_CORE_UTIL_SCOPEDPTR_HPP

namespace loka
{
  namespace core
  {
    template <typename T>
    class ScopedPtr
    {
      T *ptr_;

    public:
      explicit ScopedPtr(T *p = 0) : ptr_(p) {}
      ~ScopedPtr() { delete ptr_; }
      T *operator->() const { return ptr_; }
      T &operator*() const { return *ptr_; }
      T *get() const { return ptr_; }
      void reset(T *p = 0)
      {
        if (ptr_ != p)
        {
          delete ptr_;
          ptr_ = p;
        }
      }
      T *release()
      {
        T *tmp = ptr_;
        ptr_ = 0;
        return tmp;
      }

    private:
      ScopedPtr(const ScopedPtr &);
      ScopedPtr &operator=(const ScopedPtr &);
    };
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_UTIL_SCOPEDPTR_HPP
