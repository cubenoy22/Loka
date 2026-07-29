#ifndef LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP
#define LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP

#include <cstddef>

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        /** What the destination context can answer about itself. One entry per
            declared axis.
            `present[a] == false` means the target cannot answer that axis at
            all, which is not the same as any value it could hold — a row that
            writes an axis the target cannot answer is dropped rather than
            matched (#189's absent rule, #185 §14). */
        struct Facts
        {
          Facts()
          {
            for (std::size_t i = 0; i < kMaxAxes; ++i)
            {
              present[i] = false;
              value[i] = 0;
            }
          }

          bool present[kMaxAxes];
          /** Enum axis: the declared vocabulary value; the reader maps it to
              the package's physical slot. Scalar axis: the measured number
              itself, for example a scale percentage. */
          U32 value[kMaxAxes];
        };

        /** One selected representation.

            The truth is `(bag, offsetInBag, length)` -- a range inside a bag
            the application loaded. `bytes` is derived from it
            (`bagBase[bag] + offsetInBag`) and is only a convenience view that
            lives as long as that bag stays open; closing a bag invalidates
            everything handed out of it (#185 §14). Keeping one of the two
            sides derived is what keeps them from becoming two truths. A
            zero-length asset reports `bytes == 0`: an empty bag has a null
            base and pointer arithmetic on it would be a lie about an address
            that does not exist. */
        struct Asset
        {
          Asset()
              : bytes(0),
                length(0),
                bag(0),
                offsetInBag(0),
                kind(ASSET_KIND_UNKNOWN)
          {
          }

          const unsigned char *bytes;
          std::size_t length;
          std::size_t bag;
          std::size_t offsetInBag;
          AssetKind kind;
        };

        /** Where a package's bytes come from, for the reader that does not
            hold them all at once.

            Deliberately narrow: positioned whole reads and a total. There is
            no partial-read concept, because every caller of a partial read
            has to invent the same loop and one of them will get it wrong.
            A `false` answer says nothing about why -- naming the reason is
            the reader's job, and it has the format's expectations to name it
            with. */
        class ByteSource
        {
        public:
          virtual ~ByteSource() {}

          /** Copies [at, at+n) into dst. Complete read or false -- there is no
              partial-read concept. false does not distinguish EOF from a
              device error; that distinction belongs to the reader's
              refusals. */
          virtual bool readAt(std::size_t at, unsigned char *dst, std::size_t n) = 0;

          /** False when the total cannot be represented in std::size_t
              (32-bit host x >4GB file). */
          virtual bool size(std::size_t &out) = 0;
        };

        /** Reader for a compiled package. A reader, never a resolver: it opens,
            validates, loads a bag, and serves `get(id, facts)` from memory
            (#185 §1, §7).

            **The reader borrows; it never owns.** `openBorrowedBytes` keeps the
            caller's pointer and hands out pointers into it, because §7's whole
            point is that a loaded bag is served without a copy -- a 4 MB target
            cannot afford the alternative. That makes the buffer's lifetime part
            of the contract rather than an implementation detail: the bytes
            passed to `openBorrowedBytes` must outlive the reader and everything
            it returned. The name says "borrowed" so no call site has to infer
            it.

            The file-backed entry point borrows in exactly the same way, one
            level down: `beginOpen`/`finishOpen` keep the `ByteSource` they
            were handed and read bags out of it later, so **a committed
            source must outlive the reader or the open that replaces it** --
            the same sentence as the borrowed bytes, about a different
            object.

            Opening from a source is two calls because the reader refuses to
            allocate: `beginOpen` validates the head and reports how many
            bytes the index needs, the application provides them, and
            `finishOpen` parses. Between the two, `isOpen()`, `bagCount()`
            and `get()` still answer for the previously committed package and
            nothing about the half-finished one is observable. That is what
            failure atomicity means here -- a refused reload is a reload that
            never happened. */
        class Reader
        {
        public:
          enum IntegrityMode
          {
            VERIFY_INTEGRITY = 0,
            SKIP_INTEGRITY
          };

          enum OpenResult
          {
            OPEN_OK = 0,
            /** The form header is not "LRPK", or the buffer is too short to
                hold the fixed head. */
            OPEN_NOT_A_PACKAGE,
            OPEN_UNSUPPORTED_VERSION,
            /** The borrowed host buffer cannot be represented by the
                format's 32-bit total-length field. */
            OPEN_SIZE_OUT_OF_RANGE,
            /** The size recorded in HEAD disagrees with the buffer -- the
                cheapest and most effective check there is (#185 §10). */
            OPEN_TRUNCATED,
            OPEN_HEAD_CORRUPT,
            OPEN_INDEX_CORRUPT,
            /** V1 knows the exact required and optional chunk set. Case does
                not demote an unknown chunk to ignorable. */
            OPEN_UNKNOWN_CHUNK,
            /** The package was built against a different id space than the
                header the application compiled against. Never waived, not even
                when integrity verification is skipped: it happens daily and
                its symptom is silent
                (#185 §10). */
            OPEN_ID_SPACE_MISMATCH,
            OPEN_MALFORMED_INDEX,
            /** The borrowed base address cannot preserve the format's payload
                alignment guarantee. */
            OPEN_MISALIGNED_BUFFER,
            /** The `ByteSource` could not answer. Deliberately distinct from
                `OPEN_TRUNCATED`: truncation is the file disagreeing with its
                own declared length, which is a fact about the package, while
                this is the transport failing to deliver bytes the format says
                are there. Merging them would report a disconnected volume as
                a corrupt package. */
            OPEN_SOURCE_FAILED,
            /** `finishOpen` without a `beginOpen` to finish -- including a
                second `finishOpen`, since a pending is consumed by the first
                one whether it succeeded or not. */
            OPEN_NO_PENDING,
            /** The buffer handed to `finishOpen` is not the size `beginOpen`
                asked for. The size is not a hint the application may round
                up: the parser reads the whole span as the index. */
            OPEN_INDEX_BUFFER_SIZE_MISMATCH
          };

          enum GetResult
          {
            GET_OK = 0,
            GET_NO_SUCH_ID,
            /** The application has not opened the bag this id lives in. This is
                not asset absence -- that is impossible by construction -- it is
                a different failure and must not be shown as the same one
                (#185 §14). */
            GET_BAG_NOT_OPEN,
            /** Selection did not end with exactly one row. Well-formed
                packages cannot reach this: `lrpc` requires an axis-free
                default and refuses same-bag ambiguity, while `openBag()`
                refuses cross-bag overlap. Kept because a reader may not
                assume its input is well formed. */
            GET_NO_MATCHING_REP
          };

          enum BagResult
          {
            BAG_OK = 0,
            BAG_NO_SUCH_BAG,
            BAG_CONTENTS_CORRUPT,
            BAG_UNSUPPORTED_CODEC,
            /** This bag shares an asset id with a bag already open. Selection
                never chooses between bags, so the second open is refused. */
            BAG_ASSET_ID_CONFLICT,
            /** The wrong door for this package's backing: `openBag` on a
                stream-backed package, or `readBagInto` on a memory-backed
                one. There is nothing sensible to do instead -- a memory
                package has no source to read from and a stream package has no
                resident payload to point at -- so the mistake is named rather
                than quietly redirected. */
            BAG_WRONG_BACKING,
            /** `readBagInto` on a bag that is already open. The memory path
                treats a repeat open as idempotent because it would hand back
                the same address; here it would silently swap `bagBase` to a
                different buffer and strand every pointer already handed out.
                Close the bag first. */
            BAG_ALREADY_OPEN,
            /** `dstSize` is not the bag's stored size. Ask `bagStoredSize`. */
            BAG_BUFFER_SIZE_MISMATCH,
            /** The destination cannot preserve the format's payload alignment,
                so rows inside the bag would be handed out misaligned. */
            BAG_MISALIGNED_BUFFER,
            /** The `ByteSource` could not deliver the bag's bytes. */
            BAG_SOURCE_FAILED
          };

          Reader();

          /** Validates the head, the version-required chunk set, and every
              structural invariant. CRC verification follows `integrityMode`;
              format validation and id-space identity never do. Nothing is
              decoded and no bag is loaded.

              Failure-atomic: a reader already holding a good package keeps it,
              open bags and all, if a reload is refused. Parsing happens into a
              separate state which is committed only on `OPEN_OK`.

              @param bytes The whole package. **Borrowed** -- it must outlive
                           this reader and every `Asset` obtained from it, and
                           its address must be `kPayloadAlign`-aligned.
              @param integrityMode Whether CRC values are verified. The
                                   package always carries them and cannot
                                   disable its own inspection. */
          OpenResult openBorrowedBytes(const unsigned char *bytes,
                                       std::size_t size,
                                       U32 expectedIdSpaceStamp,
                                       IntegrityMode integrityMode);

          /** First half of the file-backed open: validates the whole fixed
              head against `src`, then measures the index by probing the AXES
              and INDX chunk headers, and reports how many bytes the
              application must hand back.

              The measured span is the file's `[512, DATA payload start)` --
              chunk headers included, DATA's header included, DATA's payload
              excluded. It exists as one contiguous run only because chunk
              order is canonical, which is precisely why the order was made
              canonical.

              Nothing is committed here. A previously opened package keeps
              answering every query, and this pending is discarded by the next
              `beginOpen`, by `openBorrowedBytes`, or by `close()` -- there is
              never more than one.

              @param src **Borrowed.** If this open commits, the source must
                         outlive the reader or the open that replaces it,
                         because bags are read from it long afterwards. The
                         borrow covers the bytes, not just the object: while
                         the open is committed, the source must keep
                         answering with the committed package's bytes.
                         Rebinding it -- reopening a `StdioByteSource` on
                         another file, say -- is the same misuse as
                         overwriting a borrowed buffer, and just as
                         unobservable from here (#220's ruling).
              @param indexBytesNeeded Set on `OPEN_OK`; zeroed otherwise. */
          OpenResult beginOpen(ByteSource &src,
                               U32 expectedIdSpaceStamp,
                               IntegrityMode integrityMode,
                               std::size_t &indexBytesNeeded);

          /** Second half: parses the index out of the buffer `beginOpen`
              asked for and, only if every check passes, commits it together
              with the pending source.

              The buffer is **borrowed** on the same terms as
              `openBorrowedBytes` -- the reader points into it for the life of
              the package. It has no alignment requirement, because every read
              inside it is bytewise and nothing in it is handed out; the
              alignment gates live where addresses do leave the reader (the
              borrowed base and `readBagInto`'s destination).

              One shot: the pending is consumed whether this succeeds or
              fails, so a refused finish is retried from `beginOpen`, never by
              calling this again with a different buffer. A second call
              answers `OPEN_NO_PENDING`. */
          OpenResult finishOpen(unsigned char *indexBuffer,
                                std::size_t indexBufferSize);

          void close();

          bool isOpen() const { return state_.backing != BACKING_NONE; }
          bool verifiesIntegrity() const { return state_.verifyIntegrity; }
          std::size_t bagCount() const { return state_.bagCount; }
          std::size_t assetCount() const { return state_.assetCount; }
          U32 idSpaceStamp() const { return state_.idSpaceStamp; }

          /** Verifies the bag against its recorded CRC and makes its rows
              addressable. Memory-backed only, and free of copies: the bytes
              are already there, so opening a bag is a CRC pass and a pointer
              (#185 §10). Repeating an open is idempotent -- the answer would
              be the same address.

              A stream-backed package answers `BAG_WRONG_BACKING`; use
              `readBagInto`. */
          BagResult openBag(std::size_t bagIndex);

          /** The stream-backed twin of `openBag`: reads the bag into the
              application's buffer, verifies it there, and installs that
              buffer as the bag's base.

              The CRC is computed over `dst` after the read, not accumulated
              during it -- a `ByteSource` is one opaque positioned read, so
              there is no "while reading" to hook. It costs no extra I/O
              either way. An empty bag is still CRC'd, so the format's check
              value is honoured for every bag rather than for most of them.

              Refuses in the same order and with the same reasons as
              `openBag`, plus the ones only a copy can have: size, alignment,
              and transport. On any refusal **the contents of `dst` are
              unspecified** -- the read may have written part of the bag, or
              all of it before the CRC disagreed. That is why the application
              seals its `Blob` only after `BAG_OK`; the same buffer may simply
              be retried, since success rewrites every byte.

              @param dst **Borrowed** for as long as the bag stays open, and
                         `kPayloadAlign`-aligned. May be null only for a bag
                         whose stored size is zero. */
          BagResult readBagInto(std::size_t bagIndex,
                                unsigned char *dst,
                                std::size_t dstSize);

          /** How big a buffer `readBagInto` wants. False, with `out` zeroed,
              when there is no such bag -- so the query doubles as existence,
              and a zero-sized bag is not confused with a missing one. */
          bool bagStoredSize(std::size_t bagIndex, std::size_t &out) const;

          void closeBag(std::size_t bagIndex);
          bool isBagOpen(std::size_t bagIndex) const;

          GetResult get(U32 id, const Facts &facts, Asset &out) const;

        private:
          /** Where the committed package's bytes are. Named rather than
              inferred: `bytes != 0` used to stand in for "open", which was a
              rumour only the memory path could tell the truth about. */
          enum Backing
          {
            BACKING_NONE = 0,
            BACKING_MEMORY,
            BACKING_STREAM
          };

          struct State;

          struct Bag
          {
            Bag()
                : dataOffset(0),
                  storedSize(0),
                  expandedSize(0),
                  crc(0),
                  codec(CODEC_NONE),
                  open(false)
            {
            }

            U32 dataOffset;
            U32 storedSize;
            U32 expandedSize;
            U32 crc;
            unsigned char codec;
            bool open;
          };

          struct Axis
          {
            Axis()
                : kind(AXIS_KIND_ENUM),
                  valueCount(0),
                  baseline(0)
            {
              for (std::size_t i = 0; i < kMaxAxisValues; ++i)
              {
                values[i] = 0;
              }
            }

            unsigned char kind;
            unsigned char valueCount;
            U32 baseline;
            U32 values[kMaxAxisValues];
          };

          /** Returns the index of the first row with this id, or assetCount_ if
              absent. Rows are sorted by id, so this is a binary search; ids are
              sparse inside a package because numbering runs over the whole tree
              (#185 §14). */
          /** Everything the fixed head decides, for both transports. Split
              out of `openBorrowedBytes` so `beginOpen` cannot drift into a
              second opinion about what a valid head is. `head512` is the
              whole fixed head; `totalSize` is the package's real length,
              which the head is checked against. */
          static OpenResult ValidateHead(const unsigned char *head512,
                                         std::size_t totalSize,
                                         U32 expectedIdSpaceStamp,
                                         bool verify,
                                         U32 &stamp,
                                         std::size_t &declaredAssets,
                                         std::size_t &declaredBags);

          /** The one chunk-stream grammar, walked in the rebased domain
              (offsets counted from the end of the fixed head) against two
              bounds:

                resident  how many chunk-stream bytes `base` actually holds
                logical   how many the format says the stream has in total

              Memory-backed packages hold everything, so they pass
              `resident == logical`. The file-backed open holds only the
              slice through the DATA chunk header, so its `resident` stops
              short. Existence checks go against resident, extent checks --
              does this payload fit the file the format describes -- against
              logical. Same function, same expressions, both transports:
              that is what "one grammar" has to mean to be worth saying. */
          static OpenResult ParseChunkStream(State &next,
                                             const unsigned char *base,
                                             std::size_t resident,
                                             std::size_t logical,
                                             const unsigned char *headPayload,
                                             bool verify,
                                             std::size_t declaredAssets,
                                             std::size_t declaredBags);

          /** Reads one chunk header at the rebased offset `at` and reports
              the whole chunk's span and its claimed payload size before
              padding. Mirrors, one chunk at a time, what `ParseChunkStream`
              would conclude about the same bytes, so a package refused by one
              transport is refused by the other for the same reason. Bounds
              are tested before the read: a false from `readAt` may only ever
              mean "bytes the format promised could not be delivered", never
              "we asked past the end". Both outputs are zero on refusal. */
          static OpenResult ProbeChunkHeader(ByteSource &src,
                                             std::size_t at,
                                             std::size_t logical,
                                             U32 expectedTag,
                                             std::size_t &spanOut,
                                             std::size_t &payloadSizeOut);

          /** The commit-time refusals a bag has regardless of how its bytes
              arrive: an id it shares with an open bag, and a codec this
              version cannot expand. Shared so both doors ask the same
              questions in the same order. */
          BagResult refuseBagConflictOrCodec(std::size_t bagIndex) const;

          std::size_t findFirstRow(U32 id) const;
          const unsigned char *assetRow(std::size_t index) const;
          bool rowIsEligible(const unsigned char *row, const Facts &facts) const;
          bool rowMatchesFilters(const unsigned char *row,
                                 const bool *filtered,
                                 const bool *enumRequiresWritten,
                                 const U32 *scalarValues) const;
          U32 rowScalarValue(const unsigned char *row, std::size_t axis) const;
          static U32 RowAxisIndex(const unsigned char *row, std::size_t axis);

          /** Everything parsing produces. Held as one value so a refused
              reload cannot leave the reader half-updated: `openBorrowedBytes`
              fills a local State and assigns it only after every check passes. */
          struct State
          {
            State()
                : backing(BACKING_NONE),
                  source(0),
                  bytes(0),
                  size(0),
                  indexBase(0),
                  indexSize(0),
                  verifyIntegrity(false),
                  idSpaceStamp(0),
                  dataPayload(0),
                  dataPayloadSize(0),
                  dataPayloadFileOffset(0),
                  assetRows(0),
                  bagCount(0),
                  assetCount(0),
                  axisCount(0)
            {
              for (std::size_t i = 0; i < kMaxAxes; ++i)
              {
                precedenceSlots[i] = 0;
              }
              for (std::size_t i = 0; i < kMaxBags; ++i)
              {
                bagBase[i] = 0;
              }
            }

            Backing backing;
            /** The source a stream-backed package reads bags from, kept by
                the committed state rather than by the reader, so a refused
                reload cannot take the live package's source away from it.
                Null for memory-backed packages. */
            ByteSource *source;
            const unsigned char *bytes;
            std::size_t size;
            /** The chunk-stream bytes the parse produced pointers into: the
                package itself past the fixed head when memory-backed, the
                application's index buffer when stream-backed. Held as a named
                range so a reload can assert it is not being asked to parse
                over the live package's own index. */
            const unsigned char *indexBase;
            std::size_t indexSize;
            bool verifyIntegrity;
            U32 idSpaceStamp;
            const unsigned char *dataPayload;
            std::size_t dataPayloadSize;
            /** Absolute file position of the DATA payload. Recorded by the
                scan whether or not the payload is resident, because the
                stream path never has it resident and still has to seek to
                it. */
            std::size_t dataPayloadFileOffset;
            const unsigned char *assetRows;
            std::size_t bagCount;
            std::size_t assetCount;
            std::size_t axisCount;
            unsigned char precedenceSlots[kMaxAxes];
            /** Where each open bag's expanded bytes live. Installed by the
                open that loaded the bag and cleared by closeBag, so `get()`
                has exactly one address-resolution path whichever backing the
                package came from: memory-backed installs a pointer into the
                DATA payload, the file-backed open installs the caller's bag
                buffer. Null while a bag is closed. */
            const unsigned char *bagBase[kMaxBags];
            Axis axes[kMaxAxes];
            Bag bags[kMaxBags];
          };

          /** The reader's second state: what `beginOpen` established and
              `finishOpen` still needs. Separate from `state_` on purpose --
              a package being opened must be unobservable until it commits,
              and the source it would install must not displace the committed
              one until then. */
          struct Pending
          {
            Pending()
                : source(0),
                  verify(false),
                  stamp(0),
                  totalBytes(0),
                  indexBytesNeeded(0),
                  declaredAssets(0),
                  declaredBags(0)
            {
            }

            /** Null means there is no pending. Nothing else in here is read
                while it is null, which is why discarding is one store. */
            ByteSource *source;
            bool verify;
            U32 stamp;
            std::size_t totalBytes;
            std::size_t indexBytesNeeded;
            std::size_t declaredAssets;
            std::size_t declaredBags;
            /** The head `beginOpen` already read and validated. Kept because
                `finishOpen` verifies the chunk CRCs against check values that
                live in it, and re-reading it would let a source answer
                differently the second time. */
            unsigned char head[kFixedHeadBytes];
          };

          /** Drops any half-finished stream open. Every entry into a new open
              -- of either kind -- calls this first, so the rule "at most one
              pending, always the most recent" needs no other statement. */
          void discardPending() { pending_.source = 0; }

          State state_;
          Pending pending_;
        };
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP
