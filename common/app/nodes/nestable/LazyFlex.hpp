#ifndef LOKA_APP_NODES_NESTABLE_LAZY_FLEX_HPP
#define LOKA_APP_NODES_NESTABLE_LAZY_FLEX_HPP

#include <climits>
#include <new>
#include "app/nodes/nestable/Canvas.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/boundary/LazyScopeDefinition.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/ObservableList.hpp"
#include "core/util/StateTrackerGuard.hpp"

#ifndef LOKA_LAZYFLEX_MAX_ITEMS
#define LOKA_LAZYFLEX_MAX_ITEMS 256
#endif

namespace loka
{
  namespace app
  {
    enum LazyFlexStatus
    {
      LAZY_FLEX_READY,
      LAZY_FLEX_CAPACITY_REFUSED
    };

    /** Value-less arm: the list and its attachment outlive the enclosing
        generation. Every materialization reads the current item value. */
    template <class T> class LazyItem : public scene::NodeDefinitionBase
    {
      typedef scene::NodeDefinition<T, typename T::NodeType> Factory;

    public:
      LazyItem(const loka::core::ObservableList<T> &list, unsigned short index)
          : list_(&list),
            index_(index)
      {
        scene::ComponentNodeWithProps<T> *required = static_cast<typename T::NodeType *>(0);
        (void)required;
      }
      loka::core::ItemId itemId() const
      {
        return this->list_->at(this->index_).id;
      }
      virtual scene::Node *create() const
      {
        return this->factory().create();
      }
      virtual scene::Node *createInPlace(void *mem) const
      {
        return this->factory().createInPlace(mem);
      }
      virtual size_t nodeSize() const
      {
        return this->factory().nodeSize();
      }
      virtual size_t nodeAlign() const
      {
        return this->factory().nodeAlign();
      }
      virtual scene::NodeKind nodeKind() const
      {
        return this->factory().nodeKind();
      }
      virtual scene::NodeDefinitionBase *clone() const
      {
        return new (std::nothrow) LazyItem(*this);
      }
      virtual const scene::PropsBase *propsBase() const
      {
        // A temporary factory's props pointer would dangle on return.
        return &this->list_->at(this->index_).value;
      }
      virtual bool hasEquivalentProps(const scene::NodeDefinitionBase &other) const
      {
        return this->factory().hasEquivalentProps(other);
      }
      virtual bool applyPropsToNode(scene::Node *node) const
      {
        return this->factory().applyPropsToNode(node);
      }
      virtual bool isCompatibleWithNode(const scene::Node *node) const
      {
        return this->factory().isCompatibleWithNode(node);
      }

    private:
      Factory factory() const
      {
        Factory value(this->list_->at(this->index_).value);
        value.copyTestIdPolicyFrom(*this);
        return value;
      }
      const loka::core::ObservableList<T> *const list_;
      const unsigned short index_;
    };

    namespace lazy_flex_detail
    {
      /** Half-open intersection, unlike Canvas's conservative far-edge visit.
          Differences use unsigned arithmetic after ordering, so extreme view
          origins cannot overflow. Geometry mirrors Canvas's declaration order. */
      class Visible : public loka::core::DerivedState<bool>::EvalFn
      {
      public:
        Visible(loka::core::State<loka::core::Frame> *viewport, unsigned short index, const CanvasProps &cells)
            : viewport_(viewport),
              cell_(cell(index, cells))
        {
        }
        virtual bool operator()()
        {
          const loka::core::Frame view = this->viewport_->get();
          return this->cell_.hasSize() && view.hasSize()
                 && overlap(this->cell_.x, this->cell_.width, view.x, view.width)
                 && overlap(this->cell_.y, this->cell_.height, view.y, view.height);
        }

      private:
        static bool overlap(int a, int aw, int b, int bw)
        {
          return a >= b ? static_cast<unsigned>(a) - static_cast<unsigned>(b) < static_cast<unsigned>(bw)
                        : static_cast<unsigned>(b) - static_cast<unsigned>(a) < static_cast<unsigned>(aw);
        }
        static loka::core::Frame cell(unsigned short index, const CanvasProps &p)
        {
          if (!p.wrap || p.cellWidth <= 0 || p.cellHeight <= 0
              || (p.axis != STACK_AXIS_COLUMN && p.axis != STACK_AXIS_ROW))
            return loka::core::Frame();
          const unsigned x = p.axis == STACK_AXIS_COLUMN ? index % p.wrap : index / p.wrap;
          const unsigned y = p.axis == STACK_AXIS_COLUMN ? index / p.wrap : index % p.wrap;
          if (x > static_cast<unsigned>(INT_MAX / p.cellWidth) || y > static_cast<unsigned>(INT_MAX / p.cellHeight))
            return loka::core::Frame();
          return loka::core::Frame(x * p.cellWidth, y * p.cellHeight, p.cellWidth, p.cellHeight);
        }
        loka::core::State<loka::core::Frame> *const viewport_;
        const loka::core::Frame cell_;
      };
    } // namespace lazy_flex_detail

    template <class T> class LazyGenerationNode;
    template <class T> struct LazyGenerationProps : scene::NodePropsBase<LazyGenerationProps<T> >
    {
      typedef LazyGenerationProps<T> TypeTag;
      typedef LazyGenerationNode<T> NodeType;
      LazyGenerationProps(const loka::core::ObservableList<T> *source, const CanvasProps &geometry)
          : list(source),
            cells(geometry)
      {
      }
      bool operator<(const scene::PropsBase &) const
      {
        return false;
      }
      const loka::core::ObservableList<T> *list;
      CanvasProps cells;
    };

    /** One candidate's visibility residents. Derived residents connect after
        constructor registrations, inside the LazyScope declaring window. */
    template <class T> class LazyGenerationNode : public scene::LazyScopeNode
    {
    public:
      typedef LazyGenerationProps<T> Props;
      typedef typename Props::TypeTag TypeTag;
      Props props;
      explicit LazyGenerationNode(const Props &p)
          : props(p),
            viewportCopy_(),
            count_(p.list->size()),
            visible_(new(std::nothrow) loka::core::State<bool> *[this->count_]())
      {
        this->declareStates(1).state(this->viewportCopy_, loka::core::Frame());
        if (!this->visible_ && this->count_)
          this->asStateOwner()->noteStateAllocationFailure();
      }
      virtual ~LazyGenerationNode()
      {
        delete[] this->visible_;
      }
      virtual void declareBindings(scene::BindingToken &token)
      {
        token.watch(*this->props.cells.viewport, this, &LazyGenerationNode::copyViewport, true);
        for (unsigned short i = 0; i < this->count_; ++i)
        {
          if (this->visible_[i])
            continue;
          lazy_flex_detail::Visible *eval =
              new (std::nothrow) lazy_flex_detail::Visible(this->viewportCopy_.state(), i, this->props.cells);
          loka::core::DerivedState<bool> *value =
              eval ? loka::core::LokaNew<loka::core::DerivedState<bool> >(
                         scene::HeapStateAllocationSite(), this->viewportCopy_.state(), eval)
                   : 0;
          if (!value)
          {
            delete eval;
            this->asStateOwner()->noteStateAllocationFailure();
            return;
          }
          value->setGateAllocated(true);
          this->asStateOwner()->adoptState(value);
          this->visible_[i] = value;
        }
      }
      virtual void declareScope(scene::NodeComposition &c)
      {
        CanvasProps cells(this->props.cells);
        cells.viewport = this->viewportCopy_.state();
        Canvas canvas(cells);
        for (unsigned short i = 0; i < this->count_; ++i)
          canvas << (Show(*this->visible_[i]).destroyOnDetach() << LazyItem<T>(*this->props.list, i));
        c.declare(canvas);
      }

    private:
      void copyViewport()
      {
        loka::core::StateTrackerGuard guard(this->asStateOwner()->tracker());
        this->viewportCopy_.set(this->props.cells.viewport->get());
      }
      scene::NodeState<loka::core::Frame> viewportCopy_;
      const unsigned short count_;
      loka::core::State<bool> **const visible_;
    };

    template <class T> class LazyFlexNode;
    /** Mounted inputs are fixed values; list contents and viewport are live
        borrows. Both owners and the list attachment must outlive this node. */
    template <class T> struct LazyFlexProps : scene::NodePropsBase<LazyFlexProps<T> >
    {
      typedef LazyFlexProps<T> TypeTag;
      typedef LazyFlexNode<T> NodeType;
      explicit LazyFlexProps(const loka::core::ObservableList<T> &source)
          : list(&source),
            cells()
      {
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != this->propsTypeId())
          return false;
        const LazyFlexProps &other = static_cast<const LazyFlexProps &>(rhs);
        if (this->list != other.list)
          return this->list < other.list;
        return this->cells < other.cells;
      }
      const loka::core::ObservableList<T> *list;
      CanvasProps cells;
    };

    /** Stable list owner. Foreign viewport/revision notifications copy into
        this Boundary's tracker before its generation and visibility settle. */
    template <class T> class LazyFlexNode : public scene::StdCompositionBoundaryNodeBase<LazyFlexProps<T> >
    {
      typedef scene::StdCompositionBoundaryNodeBase<LazyFlexProps<T> > Base;

    public:
      explicit LazyFlexNode(const LazyFlexProps<T> &p)
          : Base(p),
            viewport_(),
            structureKey_(),
            contentKey_()
      {
        this->declareStates(3)
            .state(this->viewport_, loka::core::Frame())
            .state(this->structureKey_, 0ul)
            .state(this->contentKey_, 0ul);
      }
      /** Settle both tracker copies before the scene publishes this update. */
      virtual bool flushViewDirtyImmediately(scene::NodeDirtyFlags) const
      {
        return false;
      }
      LazyFlexStatus status() const
      {
        return this->props.list->capacity() > LOKA_LAZYFLEX_MAX_ITEMS ? LAZY_FLEX_CAPACITY_REFUSED : LAZY_FLEX_READY;
      }
      virtual void declareBindings(scene::BindingToken &token)
      {
        if (this->status() != LAZY_FLEX_READY)
          return;
        if (this->props.cells.viewport)
          token.watch(*this->props.cells.viewport, this, &LazyFlexNode::copyViewport, true);
        // State observation mutates bindings, never the borrowed revision value.
        token.watch(const_cast<loka::core::State<loka::core::ListRevision> &>(this->props.list->revision()),
                    this,
                    &LazyFlexNode::copyRevision,
                    true);
        token.watch(*this->contentKey_.state(), this, &LazyFlexNode::refreshContent);
      }
      virtual void composeNode(scene::NodeComposition &c)
      {
        if (this->status() != LAZY_FLEX_READY)
          return;
        CanvasProps cells(this->props.cells);
        cells.viewport = this->viewport_.state();
        c.declare(scene::LazyScope(*this->structureKey_.state(), LazyGenerationProps<T>(this->props.list, cells)));
      }

    private:
      void copyViewport()
      {
        loka::core::StateTrackerGuard guard(this->asStateOwner()->tracker());
        this->viewport_.set(this->props.cells.viewport->get());
      }
      void copyRevision()
      {
        const loka::core::ListRevision revision = this->props.list->revision().get();
        loka::core::StateTrackerGuard guard(this->asStateOwner()->tracker());
        this->structureKey_.set(revision.structure);
        this->contentKey_.set(revision.content);
      }
      void refreshContent()
      {
        scene::NodeDefinitionBase *definition = this->composition().root();
        scene::IBranchSeatDefinition *seat = definition ? definition->asBranchSeatDefinition() : 0;
        // A mixed structure/content batch belongs to the replacement generation.
        if (!seat || seat->needsBranchDeclaration())
          return;
        scene::Node *generation = this->childrenHead();
        scene::Node *fragment = generation ? generation->asNestable()->childrenHead() : 0;
        scene::Node *canvas = fragment ? fragment->asNestable()->childrenHead() : 0;
        if (!canvas || !canvas->asCanvasNode())
          return;
        const loka::core::ListChange change = this->props.list->revision().get().change;
        const unsigned first = change.kind == loka::core::LIST_BATCH ? 0 : change.first;
        const unsigned end = change.kind == loka::core::LIST_BATCH ? this->props.list->size() : first + change.count;
        // Own Canvas rows: one O(first) seek, then O(count) arm visits.
        scene::Node *arm = canvas->asNestable()->childrenHead();
        unsigned i = 0;
        for (; arm && i < first; ++i)
          arm = arm->nextInComposition;
        for (; arm && i < end && i < this->props.list->size(); ++i, arm = arm->nextInComposition)
        {
          scene::Node *item = arm->asNestable()->childrenHead();
          if (item)
            LazyItem<T>(*this->props.list, i).applyPropsToNode(item);
        }
      }
      scene::NodeState<loka::core::Frame> viewport_;
      scene::NodeState<unsigned long> structureKey_;
      scene::NodeState<unsigned long> contentKey_;
    };

    template <class T> struct LazyFlex : scene::BoundaryDefinition<LazyFlexProps<T>, LazyFlexNode<T> >
    {
      typedef scene::BoundaryDefinition<LazyFlexProps<T>, LazyFlexNode<T> > Base;
      explicit LazyFlex(const loka::core::ObservableList<T> &list)
          : Base(LazyFlexProps<T>(list))
      {
      }
      LazyFlex &axis(StackAxis value)
      {
        this->props.cells.axis = value;
        return *this;
      }
      LazyFlex &cells(short width, short height)
      {
        this->props.cells.cellWidth = width;
        this->props.cells.cellHeight = height;
        return *this;
      }
      LazyFlex &wrap(unsigned short value)
      {
        this->props.cells.wrap = value;
        return *this;
      }
      LazyFlex &viewport(loka::core::State<loka::core::Frame> &value)
      {
        this->props.cells.viewport = &value;
        return *this;
      }
    };
    template <class T> inline LazyFlex<T> LazyColumn(const loka::core::ObservableList<T> &list)
    {
      return LazyFlex<T>(list).axis(STACK_AXIS_COLUMN);
    }
    template <class T> inline LazyFlex<T> LazyRow(const loka::core::ObservableList<T> &list)
    {
      return LazyFlex<T>(list).axis(STACK_AXIS_ROW);
    }
  } // namespace app
} // namespace loka
#endif
