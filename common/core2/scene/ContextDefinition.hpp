#ifndef DECLARA_CORE2_SCENE_CONTEXT_DEFINITION_HPP
#define DECLARA_CORE2_SCENE_CONTEXT_DEFINITION_HPP

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class ContextDefinitionBase
      {
      public:
        ContextDefinitionBase();
        ContextDefinitionBase(const ContextDefinitionBase &other);
        ContextDefinitionBase &operator=(const ContextDefinitionBase &other);
        virtual ~ContextDefinitionBase() {}
        int id() const { return id_; }

      private:
        int id_;
        static int nextId_;
      };

      template <class T>
      class ContextDefinition : public ContextDefinitionBase
      {
      public:
        typedef T ValueType;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_CONTEXT_DEFINITION_HPP
