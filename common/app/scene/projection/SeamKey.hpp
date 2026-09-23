#ifndef LOKA_APP_SCENE_PROJECTION_SEAM_KEY_HPP
#define LOKA_APP_SCENE_PROJECTION_SEAM_KEY_HPP

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <typename Derived, typename NodeT, typename CtxT> class RetainedNodeHandler;

      /** Permission to use a node type's projection seam. The retained handler
          supplies it when creating that node type's context. Copies carry no
          ownership, node identity, or lifetime extension. */
      template <typename NodeT> class SeamKey
      {
      public:
        SeamKey(const SeamKey &) {}

      private:
        template <typename Derived, typename N, typename CtxT> friend class RetainedNodeHandler;
        SeamKey() {}
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif
