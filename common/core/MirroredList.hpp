#ifndef LOKA_CORE_MIRRORED_LIST_HPP
#define LOKA_CORE_MIRRORED_LIST_HPP

#include "core/ObservableList.hpp"

namespace loka
{
  namespace core
  {

    enum MirrorResultKind
    {
      MIRROR_OK,
      MIRROR_COMMITTED,
      MIRROR_ALLOCATION_REFUSED,
      MIRROR_PAGE_REFUSED,
      MIRROR_STALE,
      MIRROR_MODEL_REFUSED,
      MIRROR_NOT_USABLE,
      MIRROR_NOTHING_TO_UNDO,
      MIRROR_REENTRANT,
      /** The mirror's own provisional id space (generation 65535) is used up;
          the model was not consulted. Recovery is a new mirror, not a retry. */
      MIRROR_SEQ_EXHAUSTED
    };
    /** Carries the original model refusal without conflating it with page refusal. */
    struct MirrorResult
    {
      MirrorResultKind kind;
      ListEditResult edit;
      MirrorResult(MirrorResultKind k = MIRROR_OK, ListEditResult e = EDIT_OK)
          : kind(k),
            edit(e)
      {
      }
    };

    namespace list_detail
    {
      /** One owner and one release path for the pending operation chain. The inline
          page is retained by clear; all heap pages return through the same gate. */
      template <class T> class OpLog
      {
        struct OpPage
        {
          ListOp<T> ops[8];
          OpPage *next;
          OpPage()
              : next(0)
          {
          }
        };

      public:
        OpLog()
            : count_(0)
        {
        }
        ~OpLog()
        {
          this->clear();
        }
        std::size_t count() const
        {
          return this->count_;
        }
        bool append(const ListOp<T> &op)
        {
          OpPage *page = &this->inline_;
          for (std::size_t i = 0; i < this->count_ / 8; ++i)
          {
            if (!page->next)
            {
              page->next = LokaNew<OpPage>(site());
              if (!page->next)
                return false;
            }
            page = page->next;
          }
          page->ops[this->count_ % 8] = op;
          ++this->count_;
          return true;
        }
        void pop()
        {
          assert(this->count_ > 0);
          --this->count_;
          // A now-empty heap tail uses the same release procedure as clear.
          OpPage *last = &this->inline_;
          for (std::size_t i = 0; i < (this->count_ ? (this->count_ - 1) / 8 : 0); ++i)
            last = last->next;
          this->releaseAfter(last);
        }
        void clear()
        {
          this->releaseAfter(&this->inline_);
          this->count_ = 0;
        }
        class Cursor : public ListOpCursor<T>
        {
        public:
          explicit Cursor(const OpLog &log)
              : log_(log),
                page_(0),
                index_(0)
          {
            this->rewind();
          }
          void rewind()
          {
            this->page_ = &this->log_.inline_;
            this->index_ = 0;
          }
          bool next(ListOp<T> &out)
          {
            if (this->index_ == this->log_.count_)
              return false;
            if (this->index_ && this->index_ % 8 == 0)
              this->page_ = this->page_->next;
            out = this->page_->ops[this->index_ % 8];
            ++this->index_;
            return true;
          }

        private:
          const OpLog &log_;
          const OpPage *page_;
          std::size_t index_;
        };

      private:
        OpLog(const OpLog &);
        OpLog &operator=(const OpLog &);
        static LokaAllocationSite site()
        {
          return LokaAllocationSite("MirroredList", "OpPage");
        }
        void releaseAfter(OpPage *last)
        {
          while (last->next)
          {
            OpPage *page = last->next;
            last->next = page->next;
            LokaDelete(page, site());
          }
        }
        OpPage inline_;
        std::size_t count_;
      };
    } // namespace list_detail

    /** Borrowed-model working copy. Model lifetime must contain mirror lifetime;
        detach makes the mirror unusable until a compatible attachment exists.
        The constructor's typed result is available through status(). Owners must not
        destroy a mirror during its commit notification. T follows
        ObservableList's nonallocating passive-value contract. Provisional IDs
        (generation 65535) belong only to this mirror and expire at cancel/commit.
        Recording validates before appending; only append may allocate a page.
        Undo replays the model plus retained operations only while the complete
        base revision and generation still match. Stale commit skips vanished
        real targets, preserves absolute destination indexes, and may refuse if
        those indexes no longer fit. Refusal preserves both pending ops and rows.
        Commit opens exactly the model apply guard; it adds no outer guard. */
    template <class T> class MirroredList
    {
    public:
      typedef typename ObservableList<T>::Entry Entry;
      explicit MirroredList(ObservableList<T> &model)
          : model_(model),
            working_(LokaAllocationSite("MirroredList", "items")),
            base_(),
            origin_(),
            size_(0),
            provisionalSeq_(0),
            phase_(LIST_IDLE),
            status_()
      {
        if (!this->model_.tracker_)
          this->status_ = MirrorResult(MIRROR_MODEL_REFUSED, EDIT_NOT_ATTACHED);
        else if (!this->working_.reserve(this->model_.capacity()))
          this->status_ = MirrorResult(MIRROR_ALLOCATION_REFUSED);
        else
          this->refresh();
      }
      ~MirroredList()
      {
        assert(this->phase_ == LIST_IDLE);
      }
      MirrorResult status() const
      {
        return this->status_;
      }
      MirrorResult insert(unsigned short index, const T &value, ItemId *outId = 0)
      {
        MirrorResult ready = this->ready();
        if (ready.kind != MIRROR_OK)
          return ready;
        if (this->provisionalSeq_ == 65535)
          return MirrorResult(MIRROR_SEQ_EXHAUSTED);
        ItemId id(65535, static_cast<unsigned short>(this->provisionalSeq_ + 1));
        MirrorResult result = this->record(ListOp<T>(INSERT, id, index, value));
        if (result.kind == MIRROR_OK)
        {
          ++this->provisionalSeq_;
          if (outId)
            *outId = id;
        }
        return result;
      }
      MirrorResult remove(ItemId id)
      {
        return this->record(ListOp<T>(REMOVE, id));
      }
      MirrorResult update(ItemId id, const T &value)
      {
        return this->record(ListOp<T>(UPDATE, id, 0, value));
      }
      MirrorResult move(ItemId id, unsigned short toIndex)
      {
        return this->record(ListOp<T>(MOVE, id, toIndex));
      }
      MirrorResult undo()
      {
        MirrorResult ready = this->ready();
        if (ready.kind != MIRROR_OK)
          return ready;
        if (this->isStale())
          return MirrorResult(MIRROR_STALE);
        if (this->ops_.count() == 0)
          return MirrorResult(MIRROR_NOTHING_TO_UNDO);
        this->ops_.pop();
        this->refresh();
        typename list_detail::OpLog<T>::Cursor cursor(this->ops_);
        ListOp<T> op;
        while (cursor.next(op))
          this->working_.edit(op, op.id, this->size_, true);
        return MirrorResult();
      }
      /** Cancels pending work even when the model is detached. On a capacity
          mismatch the mirror must be reconstructed; cancel still frees pages. */
      MirrorResult cancel()
      {
        if (this->phase_ != LIST_IDLE)
          return MirrorResult(MIRROR_REENTRANT);
        this->ops_.clear();
        MirrorResult ready = this->ready();
        if (ready.kind != MIRROR_OK)
        {
          this->size_ = 0;
          return ready;
        }
        this->refresh();
        return MirrorResult();
      }
      MirrorResult commit()
      {
        MirrorResult ready = this->ready();
        if (ready.kind != MIRROR_OK)
          return ready;
        CommitCursor cursor(*this);
        this->phase_ = LIST_PUBLISHING;
        ListEditResult result = this->model_.apply(cursor);
        if (result == EDIT_OK)
        {
          this->ops_.clear();
          this->refresh();
        }
        this->phase_ = LIST_IDLE;
        return result == EDIT_OK ? MirrorResult(MIRROR_COMMITTED) : MirrorResult(MIRROR_MODEL_REFUSED, result);
      }
      unsigned short size() const
      {
        return this->size_;
      }
      const Entry &at(unsigned short index) const
      {
        assert(index < this->size_);
        return this->working_.at(index);
      }
      bool isStale() const
      {
        return !this->model_.tracker_ || this->origin_ != ItemId(this->model_.generation_, 0)
               || this->base_ != this->model_.revision().get();
      }
      std::size_t opCount() const
      {
        return this->ops_.count();
      }

    private:
      /** Maps a local insert label by its ordinal among pending inserts. Scans
          only this mirror's log, O(ops) per provisional target, no mapping heap.
          Stale filtering is stable across both model apply passes. */
      class CommitCursor : public ListOpCursor<T>
      {
      public:
        explicit CommitCursor(const MirroredList &mirror)
            : mirror_(mirror),
              cursor_(mirror.ops_),
              stale_(mirror.origin_ != ItemId(mirror.model_.generation_, 0)
                     || mirror.base_.structure != mirror.model_.revision().get().structure)
        {
        }
        void rewind()
        {
          this->cursor_.rewind();
        }
        bool next(ListOp<T> &out)
        {
          while (this->cursor_.next(out))
          {
            if (out.kind != INSERT)
            {
              if (out.id.generation == 65535)
                out.id = this->resolve(out.id);
              else if (this->stale_ && this->mirror_.model_.find(out.id) < 0)
                continue;
            }
            return true;
          }
          return false;
        }

      private:
        ItemId resolve(ItemId provisional) const
        {
          typename list_detail::OpLog<T>::Cursor cursor(this->mirror_.ops_);
          unsigned long seq = this->mirror_.model_.seq_;
          ListOp<T> op;
          while (cursor.next(op))
          {
            if (op.kind == INSERT)
            {
              ++seq;
              if (op.id == provisional)
                return seq <= 65535 ? ItemId(this->mirror_.model_.generation_, static_cast<unsigned short>(seq))
                                    : ItemId();
            }
          }
          return ItemId();
        }
        const MirroredList &mirror_;
        typename list_detail::OpLog<T>::Cursor cursor_;
        const bool stale_;
      };
      friend struct testing::ObservableListAccessForTesting;
      MirroredList(const MirroredList &);
      MirroredList &operator=(const MirroredList &);
      MirrorResult ready() const
      {
        if (this->phase_ != LIST_IDLE)
          return MirrorResult(MIRROR_REENTRANT);
        if (this->status_.kind != MIRROR_OK)
          return MirrorResult(MIRROR_NOT_USABLE);
        if (!this->model_.tracker_)
          return MirrorResult(MIRROR_MODEL_REFUSED, EDIT_NOT_ATTACHED);
        if (this->working_.capacity() != this->model_.capacity())
          return MirrorResult(MIRROR_STALE);
        return MirrorResult();
      }
      MirrorResult record(const ListOp<T> &op)
      {
        MirrorResult ready = this->ready();
        if (ready.kind != MIRROR_OK)
          return ready;
        ListEditResult result = this->working_.validate(op, this->size_);
        if (result != EDIT_OK)
          return MirrorResult(MIRROR_MODEL_REFUSED, result);
        if (!this->ops_.append(op))
          return MirrorResult(MIRROR_PAGE_REFUSED);
        this->working_.edit(op, op.id, this->size_, true);
        return MirrorResult();
      }
      void refresh()
      {
        this->base_ = this->model_.revision().get();
        this->origin_ = ItemId(this->model_.generation_, 0);
        this->size_ = this->model_.size();
        this->working_.copy(this->model_.items_, this->size_, true);
      }
      ObservableList<T> &model_;
      list_detail::EntryBuffer<T> working_;
      list_detail::OpLog<T> ops_;
      ListRevision base_;
      ItemId origin_;
      unsigned short size_;
      unsigned short provisionalSeq_;
      ListPhase phase_;
      MirrorResult status_;
    };

  } // namespace core
} // namespace loka
#endif
