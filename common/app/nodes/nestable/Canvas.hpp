#ifndef LOKA_APP_NODES_NESTABLE_CANVAS_HPP
#define LOKA_APP_NODES_NESTABLE_CANVAS_HPP

#include "app/nodes/nestable/RowColumn.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace app
  {

    struct CanvasTypeTag
    {
    };
    class CanvasNode;

    /** Fixed cells in declaration order. Only viewport is live and borrowed;
        its owner must outlive this node. COLUMN wraps columns, ROW wraps rows. */
    struct CanvasProps : public scene::NodePropsBase<CanvasProps>
    {
      typedef CanvasTypeTag TypeTag;
      typedef CanvasNode NodeType;
      StackAxis axis;
      short cellWidth;
      short cellHeight;
      unsigned short wrap;
      loka::core::State<loka::core::Frame> *viewport;

      CanvasProps(short width = 1, short height = 1, loka::core::State<loka::core::Frame> *view = 0)
          : axis(STACK_AXIS_COLUMN),
            cellWidth(width),
            cellHeight(height),
            wrap(1),
            viewport(view)
      {
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != this->propsTypeId())
          return false;
        const CanvasProps &other = static_cast<const CanvasProps &>(rhs);
        if (this->axis != other.axis)
          return this->axis < other.axis;
        if (this->cellWidth != other.cellWidth)
          return this->cellWidth < other.cellWidth;
        if (this->cellHeight != other.cellHeight)
          return this->cellHeight < other.cellHeight;
        if (this->wrap != other.wrap)
          return this->wrap < other.wrap;
        return this->viewport < other.viewport;
      }
    };

    /** Outcome of the latest layout attempt; refusals apply in every build. */
    enum CanvasLayoutStatus
    {
      CANVAS_LAYOUT_READY,
      CANVAS_LAYOUT_INVALID_INPUT,
      CANVAS_LAYOUT_INT_RANGE_REFUSED,
      CANVAS_LAYOUT_SHORT_RANGE_REFUSED
    };

    class CanvasNode : public scene::NestableNode
    {
    public:
      typedef CanvasTypeTag TypeTag;
      CanvasProps props;
      explicit CanvasNode(const CanvasProps &p)
          : props(p),
            layoutStatus_(CANVAS_LAYOUT_READY)
      {
      }
      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_CANVAS;
      }
      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<CanvasNode>();
      }
      virtual CanvasNode *asCanvasNode()
      {
        return this;
      }
      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        registrar.markDirtyOnChange(this->props.viewport, scene::NODE_DIRTY_LAYOUT);
      }
      CanvasLayoutStatus layoutStatus() const
      {
        return this->layoutStatus_;
      }
      /** Projection records a completed attempt, replacing any previous refusal. */
      void recordLayoutStatus(CanvasLayoutStatus status)
      {
        this->layoutStatus_ = status;
      }

    private:
      CanvasLayoutStatus layoutStatus_;
    };

    struct CanvasDefinition : public scene::NestableNodeDefinition<CanvasProps, CanvasNode, CanvasDefinition>,
                              public scene::TestIdDslMixin<CanvasDefinition>
    {
      typedef scene::NestableNodeDefinition<CanvasProps, CanvasNode, CanvasDefinition> BaseType;
      using BaseType::operator<<;
      CanvasDefinition()
          : BaseType()
      {
      }
      explicit CanvasDefinition(const CanvasProps &p)
          : BaseType(p)
      {
      }
      CanvasDefinition(short width, short height, loka::core::State<loka::core::Frame> *viewport)
          : BaseType(CanvasProps(width, height, viewport))
      {
      }
      CanvasDefinition &axis(StackAxis value)
      {
        this->props.axis = value;
        return *this;
      }
      CanvasDefinition &wrap(unsigned short value)
      {
        this->props.wrap = value;
        return *this;
      }
    };
    typedef CanvasDefinition Canvas;

  } // namespace app
} // namespace loka
#endif
