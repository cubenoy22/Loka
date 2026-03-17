#ifndef LOKA_PERF_ARENA_MAIN_NODE_HPP
#define LOKA_PERF_ARENA_MAIN_NODE_HPP

#include <cassert>
#include <cstdio>

#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/Window.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "loka/core/Profiler.hpp"
#include "loka/core/String.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"

namespace perfarena
{
  struct SharedModel
  {
    SharedModel()
        : phase_(0),
          reorderFlip_(false),
          replaceFlip_(false),
          propsFlip_(false),
          nestedFlip_(false),
          arenaFrozen_(false),
          lastBurstIterations_(0),
          lastBurstTicks_(0),
          titleText_(loka::core::String::Literal("LokaPerfArena")),
          retainedEditText_(loka::core::String::Literal("retained edit 0")),
          selectedIndex_(0),
          statusText_(loka::core::String::Literal("phase: 0 / reorder: off / replace: off / props: off / nested: off / arena: live")),
          burstText_(loka::core::String::Literal("last burst: idle")),
          profileText_(loka::core::String::Literal("hot path: idle"))
    {
      for (int i = 0; i < 4; ++i)
      {
        this->laneBoundaries_[i] = 0;
      }
      this->refreshWindowTitle(0);
    }

    void attachLaneBoundary(int laneIndex, loka::app::scene::BoundaryNode *boundary)
    {
      if (laneIndex < 0 || laneIndex >= 4)
      {
        return;
      }
      this->laneBoundaries_[laneIndex] = boundary;
      if (this->laneBoundaries_[laneIndex])
      {
        this->laneBoundaries_[laneIndex]->setFrozen(this->arenaFrozen_.get());
      }
    }

    void advanceReorder()
    {
      this->reorderFlip_.set(!this->reorderFlip_.get());
      this->phase_.set(this->phase_.get() + 1);
      this->refreshDisplayState();
    }

    void advanceReplace()
    {
      this->replaceFlip_.set(!this->replaceFlip_.get());
      this->phase_.set(this->phase_.get() + 1);
      this->refreshDisplayState();
    }

    void advanceProps()
    {
      this->propsFlip_.set(!this->propsFlip_.get());
      this->phase_.set(this->phase_.get() + 1);
      this->refreshDisplayState();
    }

    void advanceMixed()
    {
      this->reorderFlip_.set(!this->reorderFlip_.get());
      this->replaceFlip_.set(!this->replaceFlip_.get());
      this->propsFlip_.set(!this->propsFlip_.get());
      this->nestedFlip_.set(!this->nestedFlip_.get());
      this->phase_.set(this->phase_.get() + 1);
      this->refreshDisplayState();
    }

    void advanceMixedStructuralOnly()
    {
      this->reorderFlip_.set(!this->reorderFlip_.get());
      this->replaceFlip_.set(!this->replaceFlip_.get());
      this->propsFlip_.set(!this->propsFlip_.get());
      this->nestedFlip_.set(!this->nestedFlip_.get());
    }

    void finalizeStructuralBurst(int iterations)
    {
      if (iterations <= 0)
      {
        return;
      }
      this->phase_.set(this->phase_.get() + iterations);
      this->refreshDisplayState();
    }

    void resetProfileState()
    {
      loka::core::ResetFuncProfile();
      this->lastBurstIterations_.set(0, true);
      this->lastBurstTicks_.set(0L, true);
      this->burstText_.set(loka::core::String::Literal("last burst: reset"), true);
      this->profileText_.set(loka::core::String::Literal("hot path: dump to refresh"), true);
    }

    void toggleArenaFrozen()
    {
      const bool next = !this->arenaFrozen_.get();
      this->arenaFrozen_.set(next);
      for (int i = 0; i < 4; ++i)
      {
        if (this->laneBoundaries_[i])
        {
          this->laneBoundaries_[i]->setFrozen(next);
        }
      }
      this->refreshStatusText();
    }

    void recordBurst(long ticks, int iterations)
    {
      this->lastBurstIterations_.set(iterations, true);
      this->lastBurstTicks_.set(ticks, true);
      this->burstText_.set(loka::core::String::Literal("last burst: ")
                               + loka::core::String::FromInt(iterations)
                               + loka::core::String::Literal(" steps / ticks: ")
                               + loka::core::String::FromInt(static_cast<int>(ticks)),
                           true);
    }

    void dumpProfileToStdout() const
    {
      struct StdoutStream : public loka::core::ProfileOutputStream
      {
        virtual void write(const char *data, int len)
        {
          std::fwrite(data, 1, static_cast<size_t>(len), stdout);
        }
      } out;
      std::printf("[PerfArena] ---- profile dump ----\n");
      loka::core::DumpFuncProfileToStream(out);
      std::printf("\n[PerfArena] ----------------------\n");
      std::fflush(stdout);
    }

    void captureProfileSummary()
    {
      loka::core::SortProfileSlotsByTicks();
      loka::core::String summary = loka::core::String::Literal("hot path:");
      int shown = 0;
      for (int i = 0; i < loka::core::gProfileSlotCount; ++i)
      {
        loka::core::FuncProfileSlot *slot = loka::core::gProfileSlots[i];
        if (!slot || slot->callCount == 0)
        {
          continue;
        }
        summary += loka::core::String::Literal(" ") + loka::core::String(slot->func)
                   + loka::core::String::Literal("=")
                   + loka::core::String::FromInt(static_cast<int>(slot->totalTicks));
        ++shown;
        if (shown >= 3)
        {
          break;
        }
      }
      if (shown == 0)
      {
        summary = loka::core::String::Literal("hot path: no samples");
      }
      this->profileText_.set(summary, true);
    }

    void refreshWindowTitle(::Window *window)
    {
      size_t liveCount = 0;
      if (window && window->scene())
      {
        liveCount = window->scene()->liveNodeCount();
      }
      this->titleText_.set(loka::core::String::Literal("LokaPerfArena [ui: ")
                               + loka::core::String::FromInt(static_cast<int>(liveCount))
                               + loka::core::String::Literal("]"),
                           true);
    }

    void refreshStatusText()
    {
      const int phase = this->phase_.get();
      this->statusText_.set(loka::core::String::Literal("phase: ")
                                + loka::core::String::FromInt(phase)
                                + loka::core::String::Literal(" / reorder: ")
                                + (this->reorderFlip_.get() ? loka::core::String::Literal("on") : loka::core::String::Literal("off"))
                                + loka::core::String::Literal(" / replace: ")
                                + (this->replaceFlip_.get() ? loka::core::String::Literal("on") : loka::core::String::Literal("off"))
                                + loka::core::String::Literal(" / props: ")
                                + (this->propsFlip_.get() ? loka::core::String::Literal("on") : loka::core::String::Literal("off"))
                                + loka::core::String::Literal(" / nested: ")
                                + (this->nestedFlip_.get() ? loka::core::String::Literal("on") : loka::core::String::Literal("off"))
                                + loka::core::String::Literal(" / arena: ")
                                + (this->arenaFrozen_.get() ? loka::core::String::Literal("frozen") : loka::core::String::Literal("live")),
                            true);
    }

    void refreshDisplayState()
    {
      const int phase = this->phase_.get();
      this->retainedEditText_.set(loka::core::String::Literal("retained edit ")
                                      + loka::core::String::FromInt(phase),
                                  true);
      this->selectedIndex_.set(phase % 4, true);
      this->refreshStatusText();
    }

    static const char **popupItems()
    {
      static const char *kItems[] = {"Alpha", "Bravo", "Charlie", "Delta"};
      return kItems;
    }

    loka::core::MutableState<int> phase_;
    loka::core::MutableState<bool> reorderFlip_;
    loka::core::MutableState<bool> replaceFlip_;
    loka::core::MutableState<bool> propsFlip_;
    loka::core::MutableState<bool> nestedFlip_;
    loka::core::MutableState<bool> arenaFrozen_;
    loka::core::MutableState<int> lastBurstIterations_;
    loka::core::MutableState<long> lastBurstTicks_;
    loka::core::MutableState<loka::core::String> titleText_;
    loka::core::MutableState<loka::core::String> retainedEditText_;
    loka::core::MutableState<int> selectedIndex_;
    loka::core::MutableState<loka::core::String> statusText_;
    loka::core::MutableState<loka::core::String> burstText_;
    loka::core::MutableState<loka::core::String> profileText_;
    loka::app::scene::BoundaryNode *laneBoundaries_[4];
  };

  struct ControlNode;
  struct ArenaShellNode;
  struct ArenaSurfaceNode;
  struct LaneNode;
  struct NestedProbeNode;

  template <class NodeT>
  struct SharedModelPropsBase : public loka::app::scene::NodePropsBase< SharedModelPropsBase<NodeT> >
  {
  };

  struct ControlPropsTypeTag
  {
  };
  struct ControlProps : public loka::app::scene::NodePropsBase<ControlProps>
  {
    typedef ControlPropsTypeTag TypeTag;
    typedef ControlNode NodeType;

    SharedModel *shared_;

    ControlProps() : shared_(0) {}
    explicit ControlProps(SharedModel *shared) : shared_(shared) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const ControlProps &other = static_cast<const ControlProps &>(rhs);
      return this->shared_ < other.shared_;
    }
  };

  struct ArenaShellPropsTypeTag
  {
  };
  struct ArenaShellProps : public loka::app::scene::NodePropsBase<ArenaShellProps>
  {
    typedef ArenaShellPropsTypeTag TypeTag;
    typedef ArenaShellNode NodeType;

    SharedModel *shared_;

    ArenaShellProps() : shared_(0) {}
    explicit ArenaShellProps(SharedModel *shared) : shared_(shared) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const ArenaShellProps &other = static_cast<const ArenaShellProps &>(rhs);
      return this->shared_ < other.shared_;
    }
  };

  struct ArenaSurfacePropsTypeTag
  {
  };
  struct ArenaSurfaceProps : public loka::app::scene::NodePropsBase<ArenaSurfaceProps>
  {
    typedef ArenaSurfacePropsTypeTag TypeTag;
    typedef ArenaSurfaceNode NodeType;

    SharedModel *shared_;

    ArenaSurfaceProps() : shared_(0) {}
    explicit ArenaSurfaceProps(SharedModel *shared) : shared_(shared) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const ArenaSurfaceProps &other = static_cast<const ArenaSurfaceProps &>(rhs);
      return this->shared_ < other.shared_;
    }
  };

  struct LanePropsTypeTag
  {
  };
  struct LaneProps : public loka::app::scene::NodePropsBase<LaneProps>
  {
    typedef LanePropsTypeTag TypeTag;
    typedef LaneNode NodeType;

    SharedModel *shared_;
    int laneIndex_;

    LaneProps() : shared_(0), laneIndex_(0) {}
    LaneProps(SharedModel *shared, int laneIndex) : shared_(shared), laneIndex_(laneIndex) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const LaneProps &other = static_cast<const LaneProps &>(rhs);
      if (this->shared_ != other.shared_)
      {
        return this->shared_ < other.shared_;
      }
      return this->laneIndex_ < other.laneIndex_;
    }
  };

  struct NestedProbePropsTypeTag
  {
  };
  struct NestedProbeProps : public loka::app::scene::NodePropsBase<NestedProbeProps>
  {
    typedef NestedProbePropsTypeTag TypeTag;
    typedef NestedProbeNode NodeType;

    SharedModel *shared_;
    int laneIndex_;

    NestedProbeProps() : shared_(0), laneIndex_(0) {}
    NestedProbeProps(SharedModel *shared, int laneIndex) : shared_(shared), laneIndex_(laneIndex) {}

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const NestedProbeProps &other = static_cast<const NestedProbeProps &>(rhs);
      if (this->shared_ != other.shared_)
      {
        return this->shared_ < other.shared_;
      }
      return this->laneIndex_ < other.laneIndex_;
    }
  };

  class NestedProbeNode : public loka::app::scene::DynamicCompositionBoundaryNodeBase<NestedProbeProps>
  {
  public:
    NestedProbeNode(const NestedProbeProps &props)
        : loka::app::scene::DynamicCompositionBoundaryNodeBase<NestedProbeProps>(props),
          pulseEvent_()
    {
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      registrar.observe(&this->props.shared_->nestedFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->propsFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->phase_, loka::app::scene::NODE_DIRTY_CHILD);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ColumnDefinition column = loka::app::VStack();
      column << loka::app::Text(loka::core::String::Literal("Nested lane ")
                                + loka::core::String::FromInt(this->props.laneIndex_ + 1)
                                + loka::core::String::Literal(" / phase ")
                                + loka::core::String::FromInt(this->props.shared_->phase_.get())).tag(1);
      if (this->props.shared_->nestedFlip_.get())
      {
        loka::app::ButtonProps props;
        props.text(loka::core::String::Literal("Nested button ")
                   + loka::core::String::FromInt(this->props.laneIndex_ + 1));
        props.onClick(&this->pulseEvent_);
        props.enabled(&this->props.shared_->propsFlip_);
        column << loka::app::ButtonDefinition(props).tag(2);
      }
      else
      {
        column << loka::app::Text(loka::core::String::Literal("Nested text ")
                                  + loka::core::String::FromInt(this->props.laneIndex_ + 1)).tag(2);
      }
      c.declare(column);
    }

  private:
    loka::core::EmitterState pulseEvent_;
  };

  inline loka::app::scene::BoundaryDefinition<NestedProbeProps, NestedProbeNode> NestedProbeBoundary(const NestedProbeProps &props)
  {
    return loka::app::scene::BoundaryDefinition<NestedProbeProps, NestedProbeNode>(props);
  }

  class LaneNode : public loka::app::scene::DynamicCompositionBoundaryNodeBase<LaneProps>
  {
  public:
    LaneNode(const LaneProps &props)
        : loka::app::scene::DynamicCompositionBoundaryNodeBase<LaneProps>(props),
          laneAction_()
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      this->props.shared_->attachLaneBoundary(this->props.laneIndex_, this);
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      registrar.observe(&this->props.shared_->phase_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->reorderFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->replaceFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->propsFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->nestedFlip_, loka::app::scene::NODE_DIRTY_CHILD);
      registrar.observe(&this->props.shared_->retainedEditText_, loka::app::scene::NODE_DIRTY_PROPS);
      registrar.observe(&this->props.shared_->selectedIndex_, loka::app::scene::NODE_DIRTY_PROPS);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ColumnDefinition column = loka::app::VStack();
      column << loka::app::Text(loka::core::String::Literal("Lane ")
                                + loka::core::String::FromInt(this->props.laneIndex_ + 1)
                                + loka::core::String::Literal(" / phase ")
                                + loka::core::String::FromInt(this->props.shared_->phase_.get())).tag(10);
      if (((this->props.shared_->reorderFlip_.get() ? 1 : 0) + this->props.laneIndex_) % 2 == 0)
      {
        this->appendRetainedCore(column);
        this->appendReplaceableCore(column);
      }
      else
      {
        this->appendReplaceableCore(column);
        this->appendRetainedCore(column);
      }
      column << locaTextTagSummary().tag(90);
      c.declare(column);
    }

  private:
    loka::app::TextDefinition locaTextTagSummary() const
    {
      return loka::app::Text(loka::core::String::Literal("flags: ")
                             + (this->props.shared_->reorderFlip_.get() ? loka::core::String::Literal("R") : loka::core::String::Literal("-"))
                             + (this->props.shared_->replaceFlip_.get() ? loka::core::String::Literal("X") : loka::core::String::Literal("-"))
                             + (this->props.shared_->propsFlip_.get() ? loka::core::String::Literal("P") : loka::core::String::Literal("-"))
                             + (this->props.shared_->nestedFlip_.get() ? loka::core::String::Literal("N") : loka::core::String::Literal("-")));
    }

    void appendRetainedCore(loka::app::ColumnDefinition &column)
    {
      column << loka::app::EditText(&this->props.shared_->retainedEditText_)
                    .controlTag(400 + this->props.laneIndex_)
                    .testId("PerfArenaEdit")
                    .tag(20)
             << loka::app::PopupMenu(SharedModel::popupItems(), 4)
                    .selectedIndex(&this->props.shared_->selectedIndex_)
                    .testId("PerfArenaPopup")
                    .tag(30)
             << NestedProbeBoundary(NestedProbeProps(this->props.shared_, this->props.laneIndex_)).tag(40)
             << loka::app::Text(loka::core::String::Literal("retained props ")
                                + (this->props.shared_->propsFlip_.get() ? loka::core::String::Literal("hot") : loka::core::String::Literal("cold"))
                                + loka::core::String::Literal(" / lane ")
                                + loka::core::String::FromInt(this->props.laneIndex_ + 1)).tag(50);
    }

    void appendReplaceableCore(loka::app::ColumnDefinition &column)
    {
      if ((this->props.shared_->replaceFlip_.get() ? 1 : 0) == (this->props.laneIndex_ % 2))
      {
        column << this->replaceButtonA().tag(60);
      }
      else
      {
        column << this->replaceTextA().tag(60);
      }

      if ((this->props.shared_->replaceFlip_.get() ? 1 : 0) != (this->props.laneIndex_ % 2))
      {
        column << this->replaceButtonB().tag(70);
      }
      else
      {
        column << this->replaceTextB().tag(70);
      }

      column << this->locaActionNode().tag(80);
    }

    loka::app::ButtonDefinition locaActionNode() const
    {
      loka::app::ButtonProps props;
      props.text(loka::core::String::Literal("lane action ")
                 + loka::core::String::FromInt(this->props.laneIndex_ + 1));
      props.onClick(&const_cast<LaneNode *>(this)->laneAction_);
      props.enabled(&this->props.shared_->propsFlip_);
      return loka::app::ButtonDefinition(props);
    }

    loka::app::ButtonDefinition replaceButton(const char *prefix) const
    {
      loka::app::ButtonProps props;
      props.text(loka::core::String::Literal(prefix)
                 + loka::core::String::Literal(" ")
                 + loka::core::String::FromInt(this->props.laneIndex_ + 1));
      props.onClick(&const_cast<LaneNode *>(this)->laneAction_);
      props.enabled(&this->props.shared_->propsFlip_);
      return loka::app::ButtonDefinition(props);
    }

    loka::app::TextDefinition replaceText(const char *prefix) const
    {
      return loka::app::Text(loka::core::String::Literal(prefix)
                             + loka::core::String::Literal(" ")
                             + loka::core::String::FromInt(this->props.laneIndex_ + 1));
    }

    loka::app::ButtonDefinition replaceButtonA() const
    {
      return this->replaceButton("button-A");
    }

    loka::app::ButtonDefinition replaceButtonB() const
    {
      return this->replaceButton("button-B");
    }

    loka::app::TextDefinition replaceTextA() const
    {
      return this->replaceText("text-A");
    }

    loka::app::TextDefinition replaceTextB() const
    {
      return this->replaceText("text-B");
    }

    loka::core::EmitterState laneAction_;
  };

  inline loka::app::scene::BoundaryDefinition<LaneProps, LaneNode> LaneBoundary(const LaneProps &props)
  {
    return loka::app::scene::BoundaryDefinition<LaneProps, LaneNode>(props);
  }

  class ArenaSurfaceNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<ArenaSurfaceProps>
  {
  public:
    ArenaSurfaceNode(const ArenaSurfaceProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<ArenaSurfaceProps>(props)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::RowDefinition topRow = loka::app::HStack();
      loka::app::RowDefinition bottomRow = loka::app::HStack();
      topRow << LaneBoundary(LaneProps(this->props.shared_, 0)).tag(10)
             << LaneBoundary(LaneProps(this->props.shared_, 1)).tag(20);
      bottomRow << LaneBoundary(LaneProps(this->props.shared_, 2)).tag(30)
                << LaneBoundary(LaneProps(this->props.shared_, 3)).tag(40);
      c.declare(loka::app::VStack()
                << loka::app::Text("Perf Arena Surface").tag(1)
                << topRow
                << bottomRow);
    }
  };

  inline loka::app::scene::BoundaryDefinition<ArenaSurfaceProps, ArenaSurfaceNode> ArenaSurfaceBoundary(const ArenaSurfaceProps &props)
  {
    return loka::app::scene::BoundaryDefinition<ArenaSurfaceProps, ArenaSurfaceNode>(props);
  }

  class ArenaShellNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<ArenaShellProps>
  {
  public:
    ArenaShellNode(const ArenaShellProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<ArenaShellProps>(props),
          reorderEvent_(),
          replaceEvent_(),
          propsEvent_(),
          mixedEvent_(),
          burst25Event_(),
          burst100Event_(),
          freezeEvent_(),
          resetProfileEvent_(),
          dumpProfileEvent_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (this->initialized_)
      {
        return;
      }
      this->bindForUi(this->reorderEvent_, this, &ArenaShellNode::handleReorder);
      this->bindForUi(this->replaceEvent_, this, &ArenaShellNode::handleReplace);
      this->bindForUi(this->propsEvent_, this, &ArenaShellNode::handleProps);
      this->bindForUi(this->mixedEvent_, this, &ArenaShellNode::handleMixed);
      this->bindForUi(this->burst25Event_, this, &ArenaShellNode::handleBurst25);
      this->bindForUi(this->burst100Event_, this, &ArenaShellNode::handleBurst100);
      this->bindForUi(this->freezeEvent_, this, &ArenaShellNode::handleFreeze);
      this->bindForUi(this->resetProfileEvent_, this, &ArenaShellNode::handleResetProfile);
      this->bindForUi(this->dumpProfileEvent_, this, &ArenaShellNode::handleDumpProfile);
      this->initialized_ = true;
      this->syncWindowTitle();
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::RowDefinition stepRow = loka::app::HStack();
      loka::app::RowDefinition burstRow = loka::app::HStack();
      loka::app::RowDefinition profileRow = loka::app::HStack();
      loka::app::ColumnDefinition column = loka::app::VStack();
      stepRow << loka::app::Button("Step Reorder", &this->reorderEvent_)
              << loka::app::Button("Step Replace", &this->replaceEvent_)
              << loka::app::Button("Step Props", &this->propsEvent_)
              << loka::app::Button("Step Mixed", &this->mixedEvent_);
      burstRow << loka::app::Button("Burst 25", &this->burst25Event_)
               << loka::app::Button("Burst 100", &this->burst100Event_)
               << loka::app::Button("Toggle Freeze", &this->freezeEvent_);
      profileRow << loka::app::Button("Reset Profile", &this->resetProfileEvent_)
                 << loka::app::Button("Dump Profile", &this->dumpProfileEvent_);
      column << loka::app::Text("Loka PerfArena")
             << loka::app::Text("Heavy local-diff playground for reorder, replace, props, nested boundaries, and retained controls.")
             << loka::app::Text(&this->props.shared_->statusText_)
             << loka::app::Text(&this->props.shared_->burstText_)
             << loka::app::Text(&this->props.shared_->profileText_)
             << stepRow
             << burstRow
             << profileRow
             << ArenaSurfaceBoundary(ArenaSurfaceProps(this->props.shared_)).tag(20);
      c.declare(column);
    }

  private:
    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    void flushWindow()
    {
      ::Window *window = this->windowOrNull();
      if (window)
      {
        window->flushSceneInvalidation();
        this->syncWindowTitle();
      }
    }

    void syncWindowTitle()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      {
        loka::core::StateTrackerGuard guard(window->getTracker());
        this->props.shared_->refreshWindowTitle(window);
      }
      if (window->hasPendingScenePlatformSync())
      {
        window->synchronizeScenePlatform();
      }
    }

    void handleReorder()
    {
      this->mutateShared(&ArenaShellNode::advanceReorderCore);
    }

    void handleReplace()
    {
      this->mutateShared(&ArenaShellNode::advanceReplaceCore);
    }

    void handleProps()
    {
      this->mutateShared(&ArenaShellNode::advancePropsCore);
    }

    void handleMixed()
    {
      this->mutateShared(&ArenaShellNode::advanceMixedCore);
    }

    void handleBurst25()
    {
      this->runBurst(25);
    }

    void handleBurst100()
    {
      this->runBurst(100);
    }

    void handleFreeze()
    {
      this->mutateShared(&ArenaShellNode::toggleFreezeCore);
    }

    void handleResetProfile()
    {
      this->mutateShared(&ArenaShellNode::resetProfileCore);
    }

    void handleDumpProfile()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      loka::core::StateTrackerGuard guard(window->getTracker());
      this->props.shared_->captureProfileSummary();
      this->props.shared_->dumpProfileToStdout();
      this->flushWindow();
    }

    typedef void (ArenaShellNode::*MutateFn)();

    void mutateShared(MutateFn fn)
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      {
        loka::core::StateTrackerGuard guard(window->getTracker());
        (this->*fn)();
      }
      this->flushWindow();
    }

    void runBurst(int iterations)
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      this->props.shared_->resetProfileState();
      const long start = loka::core::ProfileTicks();
      for (int i = 0; i < iterations; ++i)
      {
        {
          loka::core::StateTrackerGuard guard(window->getTracker());
          this->props.shared_->advanceMixedStructuralOnly();
        }
        window->flushSceneInvalidation();
      }
      {
        loka::core::StateTrackerGuard guard(window->getTracker());
        this->props.shared_->finalizeStructuralBurst(iterations);
        this->props.shared_->recordBurst(loka::core::ProfileTicks() - start, iterations);
      }
      this->flushWindow();
    }

    void advanceReorderCore()
    {
      this->props.shared_->advanceReorder();
    }

    void advanceReplaceCore()
    {
      this->props.shared_->advanceReplace();
    }

    void advancePropsCore()
    {
      this->props.shared_->advanceProps();
    }

    void advanceMixedCore()
    {
      this->props.shared_->advanceMixed();
    }

    void toggleFreezeCore()
    {
      this->props.shared_->toggleArenaFrozen();
    }

    void resetProfileCore()
    {
      this->props.shared_->resetProfileState();
    }

    loka::core::EmitterState reorderEvent_;
    loka::core::EmitterState replaceEvent_;
    loka::core::EmitterState propsEvent_;
    loka::core::EmitterState mixedEvent_;
    loka::core::EmitterState burst25Event_;
    loka::core::EmitterState burst100Event_;
    loka::core::EmitterState freezeEvent_;
    loka::core::EmitterState resetProfileEvent_;
    loka::core::EmitterState dumpProfileEvent_;
    bool initialized_;
  };

  class ControlNode : public loka::app::scene::StaticCompositionBoundaryNodeBase<ControlProps>
  {
  public:
    ControlNode(const ControlProps &props)
        : loka::app::scene::StaticCompositionBoundaryNodeBase<ControlProps>(props),
          reorderEvent_(),
          replaceEvent_(),
          propsEvent_(),
          mixedEvent_(),
          burst25Event_(),
          burst100Event_(),
          freezeEvent_(),
          resetProfileEvent_(),
          dumpProfileEvent_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      this->bindForUi(this->reorderEvent_, this, &ControlNode::handleReorder);
      this->bindForUi(this->replaceEvent_, this, &ControlNode::handleReplace);
      this->bindForUi(this->propsEvent_, this, &ControlNode::handleProps);
      this->bindForUi(this->mixedEvent_, this, &ControlNode::handleMixed);
      this->bindForUi(this->burst25Event_, this, &ControlNode::handleBurst25);
      this->bindForUi(this->burst100Event_, this, &ControlNode::handleBurst100);
      this->bindForUi(this->freezeEvent_, this, &ControlNode::handleFreeze);
      this->bindForUi(this->resetProfileEvent_, this, &ControlNode::handleResetProfile);
      this->bindForUi(this->dumpProfileEvent_, this, &ControlNode::handleDumpProfile);
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::RowDefinition stepRow = loka::app::HStack();
      loka::app::RowDefinition burstRow = loka::app::HStack();
      loka::app::RowDefinition profileRow = loka::app::HStack();
      stepRow << loka::app::Button("Step Reorder", &this->reorderEvent_)
              << loka::app::Button("Step Replace", &this->replaceEvent_)
              << loka::app::Button("Step Props", &this->propsEvent_)
              << loka::app::Button("Step Mixed", &this->mixedEvent_);
      burstRow << loka::app::Button("Burst 25", &this->burst25Event_)
               << loka::app::Button("Burst 100", &this->burst100Event_)
               << loka::app::Button("Toggle Freeze", &this->freezeEvent_);
      profileRow << loka::app::Button("Reset Profile", &this->resetProfileEvent_)
                 << loka::app::Button("Dump Profile", &this->dumpProfileEvent_);
      c.declare(loka::app::VStack()
                << loka::app::Text("PerfArena Control")
                << loka::app::Text("Use burst actions to force repeated local diffs and watch retained controls stay alive.")
                << loka::app::Text(&this->props.shared_->statusText_)
                << loka::app::Text(&this->props.shared_->burstText_)
                << loka::app::Text(&this->props.shared_->profileText_)
                << stepRow
                << burstRow
                << profileRow
                << loka::app::Text("Open the arena window beside this one and trigger reorder/replace bursts while watching retained EditText and PopupMenu state persist."));
    }

  private:
    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    void flushWindows()
    {
      ::Window *controlWindow = this->windowOrNull();
      if (controlWindow)
      {
        controlWindow->flushSceneInvalidation();
      }
    }

    void handleReorder()
    {
      this->mutateShared(&ControlNode::advanceReorderCore);
    }

    void handleReplace()
    {
      this->mutateShared(&ControlNode::advanceReplaceCore);
    }

    void handleProps()
    {
      this->mutateShared(&ControlNode::advancePropsCore);
    }

    void handleMixed()
    {
      this->mutateShared(&ControlNode::advanceMixedCore);
    }

    void handleBurst25()
    {
      this->runBurst(25);
    }

    void handleBurst100()
    {
      this->runBurst(100);
    }

    void handleFreeze()
    {
      this->mutateShared(&ControlNode::toggleFreezeCore);
    }

    void handleResetProfile()
    {
      this->mutateShared(&ControlNode::resetProfileCore);
    }

    void handleDumpProfile()
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      loka::core::StateTrackerGuard guard(window->getTracker());
      this->props.shared_->captureProfileSummary();
      this->props.shared_->dumpProfileToStdout();
      this->flushWindows();
    }

    typedef void (ControlNode::*MutateFn)();

    void mutateShared(MutateFn fn)
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      {
        loka::core::StateTrackerGuard guard(window->getTracker());
        (this->*fn)();
      }
      this->flushWindows();
    }

    void runBurst(int iterations)
    {
      ::Window *window = this->windowOrNull();
      if (!window)
      {
        return;
      }
      this->props.shared_->resetProfileState();
      const long start = loka::core::ProfileTicks();
      for (int i = 0; i < iterations; ++i)
      {
        {
          loka::core::StateTrackerGuard guard(window->getTracker());
          this->props.shared_->advanceMixed();
        }
        window->flushSceneInvalidation();
      }
      {
        loka::core::StateTrackerGuard guard(window->getTracker());
        this->props.shared_->recordBurst(loka::core::ProfileTicks() - start, iterations);
      }
      this->flushWindows();
    }

    void advanceReorderCore()
    {
      this->props.shared_->advanceReorder();
    }

    void advanceReplaceCore()
    {
      this->props.shared_->advanceReplace();
    }

    void advancePropsCore()
    {
      this->props.shared_->advanceProps();
    }

    void advanceMixedCore()
    {
      this->props.shared_->advanceMixed();
    }

    void toggleFreezeCore()
    {
      this->props.shared_->toggleArenaFrozen();
    }

    void resetProfileCore()
    {
      this->props.shared_->resetProfileState();
    }

    loka::core::EmitterState reorderEvent_;
    loka::core::EmitterState replaceEvent_;
    loka::core::EmitterState propsEvent_;
    loka::core::EmitterState mixedEvent_;
    loka::core::EmitterState burst25Event_;
    loka::core::EmitterState burst100Event_;
    loka::core::EmitterState freezeEvent_;
    loka::core::EmitterState resetProfileEvent_;
    loka::core::EmitterState dumpProfileEvent_;
    bool initialized_;
  };

  inline loka::app::scene::BoundaryDefinition<ControlProps, ControlNode> ControlBoundary(const ControlProps &props)
  {
    return loka::app::scene::BoundaryDefinition<ControlProps, ControlNode>(props);
  }

  inline loka::app::scene::BoundaryDefinition<ArenaShellProps, ArenaShellNode> ArenaShellBoundary(const ArenaShellProps &props)
  {
    return loka::app::scene::BoundaryDefinition<ArenaShellProps, ArenaShellNode>(props);
  }
}

#endif // LOKA_PERF_ARENA_MAIN_NODE_HPP
