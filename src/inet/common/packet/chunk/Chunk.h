//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef __INET_CHUNK_H_
#define __INET_CHUNK_H_

#include <memory>
#include "inet/common/MemoryInputStream.h"
#include "inet/common/MemoryOutputStream.h"
#include "inet/common/Units.h"

// checking chunk implementation is disabled by default
#ifndef CHUNK_CHECK_IMPLEMENTATION_ENABLED
#define CHUNK_CHECK_IMPLEMENTATION_ENABLED 0
#endif

#if CHUNK_CHECK_IMPLEMENTATION_ENABLED
#define CHUNK_CHECK_IMPLEMENTATION(condition)   ASSERT(condition)
#else
#define CHUNK_CHECK_IMPLEMENTATION(condition) ;
#endif

// checking chunk usage is enabled in debug mode and disabled in release mode by default
#ifndef CHUNK_CHECK_USAGE_ENABLED
#ifdef NDEBUG
#define CHUNK_CHECK_USAGE_ENABLED 0
#else
#define CHUNK_CHECK_USAGE_ENABLED 1
#endif
#endif

#if CHUNK_CHECK_USAGE_ENABLED
#define CHUNK_CHECK_USAGE(condition, format, ...)   ((void) ((condition) ? 0 :  throw cRuntimeError("Error: " format, ##__VA_ARGS__)))
#else
#define CHUNK_CHECK_USAGE(condition, format, ...) ;
#endif

namespace inet {

using namespace units::values;

/**
 * This class represents a piece of data that is usually part of a packet or
 * some other data such as a protocol buffer. The chunk interface is designed
 * to be very flexible in terms of alternative representations. For example,
 * when the actual bytes in the data is irrelevant, a chunk can be as simple as
 * an integer specifying its length. Contrary, sometimes it might be necessary
 * to represent the data bytes as is. For example, when one is using an external
 * program to send or receive the data. But most often it's better to have a
 * representation that is easy to inspect and understand during debugging.
 * Fortunately, this representation can be easily generated using the omnetpp
 * message compiler. In any case, chunks can always be converted to and from a
 * sequence of bytes using the corresponding serializer.
 *
 * Chunks can represent data in different ways:
 *  - BitsChunk contains a sequence of bits
 *  - BitCountChunk contains a bit length field only
 *  - BytesChunk contains a sequence of bytes
 *  - ByteCountChunk contains a byte length field only
 *  - SliceChunk contains a slice of another chunk
 *  - SequenceChunk contains a sequence of other chunks
 *  - cPacketChunk contains a cPacket for backward compatibility
 *  - message compiler generated chunks extending FieldsChunk contain various
 *    user defined fields
 *
 * Chunks are initially:
 *  - mutable, then may become immutable, but never the other way around. There
 *    is one exception to this rule, when the chunk is exclusively owned. That
 *    is when it has exactly one reference. Immutable chunks cannot be changed
 *    anymore because they might be used to efficiently share data.
 *  - complete, then may become incomplete, but never the other way around.
 *    Incomplete chunks are not totally filled in. For example, when a chunk is
 *    created by deserializing it from insufficient amount of data.
 *  - correct, then may become incorrect, but never the other way around.
 *    Incorrect chunks contain bit errors. For example, when a packet is sent
 *    through a lossy channel causing bit errors in it.
 *  - properly represented, then may become improperly represented, but never
 *    the other way around. Improperly represented chunks misrepresent their
 *    data. For example, when a chunk is created by deserializing data but the
 *    field based representation does not fully support all possible data.
 *
 * Chunks can be safely shared using Ptr. In fact, the reason for
 * having immutable chunks is to allow for efficient sharing using shared
 * pointers. For example, when a Packet data structure is duplicated the copy
 * can share immutable chunks with the original.
 *
 * In general, chunks support the following operations:
 *  - insert to the beginning or end
 *  - remove from the beginning or end
 *  - query length and peek an arbitrary part
 *  - serialize to and deserialize from a sequence of bytes
 *  - copy to a new mutable chunk
 *  - convert to a human readable string
 *
 * The chunk API supports polymorphism, so it's possible to create chunk class
 * hierarchies with proper automatic deserialization. Chunks can be nested into
 * each other similarly how SliceChunk and SequenceChunk does it. You can find
 * examples for these in the tests folder of INET.
 *
 * Communication protocol headers often contain optional parts. There are many
 * ways to represent optional fields with chunks. For example, the IP header
 * contains an optional part that is a list of IP specific options. Assuming
 * the packet already contains a SequenceChunk, the IP header can be represented
 * in various ways. Here are a few examples:
 *
 * - IpHeader is a FieldsChunk subclass
 *   - mandatory fields are directly represented in the IpHeader chunk
 *   - each IpOption is added as an optional structure inside an array
 *
 * - IpHeader is a FieldsChunk subclass
 * - IpOptions is a SequenceChunk subclass
 *   - each IpOption is a FieldsChunk subclass inside the SequenceChunk
 *
 * - IpHeader is a FieldsChunk subclass
 * - each IpOption is added as a separate FieldsChunk subclass
 *
 * Communication protocol headers also often contain CRC or checksum fields. In
 * network simulations, most of the time it's unnecessary to compute the correct
 * CRC. One can safely assume the correct CRC is present, unless the packet has
 * to be serialized. As for the representation, a FieldsChunk subclass should
 * contain a CRC field and possibly a CRC mode field. Please note that computing,
 * inserting and verifying the actual CRC is not the task of the chunk serializer
 * but rather the task of the communication protocol. You can find a few examples
 * how to implement CRC insertion and verification in the TCP/IP protocol stack.
 *
 * Implementing lossy communication channels is somewhat more complicated but
 * also more flexible with new chunk API. Whenever a packet passes through a
 * lossy channel there are a number of options to represent errors.
 *  - the simplest way is marking the whole packet erroneous via setBitError()
 *  - a more detailed way would be iterating through the chunks in the packet
 *    and marking the erroneous chunks via markIncorrect(). Note that the chunks
 *    marked this way has to be duplicated before, and they have to be replaced
 *    in the packet to avoid accidentally propagating the same error to multiple
 *    receivers.
 *  - the most detailed way is asking for the BytesChunk (or BitsChunk) of the
 *    packet and actually changing the erroneous bytes (bits) before the packet
 *    arrives at the receiver. Fortunately, the receiver doesn't have to worry
 *    about receiving a packet containing bytes instead of the original chunks,
 *    because the chunk API provides the requested chunk types either way.
 *
 * Representation duality is a very important aspect of the chunk API. It means
 * that a protocol doesn't have to worry about the actual representation of the
 * packet's data while it's processing. For example, when the IP protocol peeks
 * the packet for the IpHeader, there are a few possiblities. The requested class
 * is either already there to be returned, or if the packet contains the raw
 * bytes, then it's going to be deserialized automatically and transparently.
 *
 * In fact, the chunk API supports turning any kind of chunk into any other kind
 * of chunk using serialization/deserialization. This represents the fact that
 * the protocol data is really a sequence of bits and any other representation
 * is just a view on top of that. Of course, this could lead to all sorts of
 * surprising things such as accidentally processing a Foo protocol header as a
 * Bar protocol header. This can happen due bit errors (e.g. incorrect length
 * field) or programming errors. To avoid hard to debug errors this conversion
 * is disabled by default.
 *
 * General rules for peeking into a chunk:
 * 1) Peeking never returns
 *    a) a SliceChunk containing
 *       - a BitsChunk
 *       - a BitCountChunk
 *       - a BytesChunk
 *       - a ByteCountChunk
 *       - another SliceChunk
 *       - the whole of another chunk
 *       - a SequenceChunk with superfluous elements
 *    b) a SequenceChunk containing
 *       - connecting BitsChunks
 *       - connecting BitCountChunks
 *       - connecting BytesChunks
 *       - connecting ByteCountChunks
 *       - connecting SliceChunks of the same chunk
 *       - another SequenceChunk
 *       - only one chunk
 * 2) Peeking (various special cases)
 *    a) an empty part of the chunk returns nullptr if allowed
 *    b) the whole of the chunk returns the chunk
 *    c) any part that is directly represented by another chunk returns that chunk
 * 3) Peeking without providing a return type for a
 *    a) BitsChunk always returns a BitsChunk containing the bits of the
 *       requested part
 *    b) BitCountChunk always returns a BitCountChunk containing the requested
 *       length
 *    c) BytesChunk always returns a BytesChunk containing the bytes of the
 *       requested part
 *    d) ByteCountChunk always returns a ByteCountChunk containing the requested
 *       length
 *    e) SliceChunk always returns a SliceChunk containing the requested slice
 *       of the chunk that is used by the original SliceChunk
 *    f) SequenceChunk may return
 *       - an element chunk
 *       - a SliceChunk of an element chunk
 *       - a SliceChunk using the original SequenceChunk
 *       - a new SequenceChunk using the elements of the original SequenceChunk
 *    g) any other chunk returns a SliceChunk
 * 4) Peeking with providing a return type always returns a chunk of the
 *    requested type (or a subtype thereof)
 *    a) Peeking with a BitCountChunk return type for any chunk returns a
 *       BitCountChunk containing the requested bit length
 *    b) Peeking with a BitsChunk return type for any chunk returns a
 *       BitsChunk containing a part of the serialized bits of the
 *       original chunk
 *    c) Peeking with a ByteCountChunk return type for any chunk returns a
 *       ByteCountChunk containing the requested byte length
 *    d) Peeking with a BytesChunk return type for any chunk returns a
 *       BytesChunk containing a part of the serialized bytes of the
 *       original chunk
 *    e) Peeking with a SliceChunk return type for any chunk returns a
 *       SliceChunk containing the requested slice of the original chunk
 *    f) Peeking with a SequenceChunk return type is an error
 *    g) Peeking with a any other return type for any chunk returns a chunk of
 *       the requested type containing data deserialized from the bytes that
 *       were serialized from the original chunk
 * 5) Peeking never returns a chunk that is one of the following unless it's
 *    explicitly allowed by passing the corresponding peek flag
 *    a) a nullptr
 *    b) an incomplete chunk
 *    c) an incorrect chunk
 *    d) an improperly represented chunk
 *    e) a chunk converted from Foo to Bar
 *
 * General rules for inserting a chunk into another:
 * a) Inserting a BitsChunk into a BitsChunk merges them
 * b) Inserting a BitCountChunk into a BitCountChunk merges them
 * c) Inserting a BytesChunk into a BytesChunk merges them
 * d) Inserting a ByteCountChunk into a ByteCountChunk merges them
 * e) Inserting a connecting SliceChunk into a SliceChunk merges them
 */
// TODO: performance related; avoid iteration in SequenceChunk::getChunkLength, avoid peek for simplifying, use vector instead of deque, reverse order for frequent prepends?
class INET_API Chunk : public cObject, public std::enable_shared_from_this<Chunk>
{
  friend class SliceChunk;
  friend class SequenceChunk;
  friend class ChunkDescriptor;

  protected:
    /**
     * This enum specifies bitmasks for the flags field of Chunk.
     */
    enum ChunkFlag {
        CF_IMMUTABLE              = (1 << 0),
        CF_INCOMPLETE             = (1 << 1),
        CF_INCORRECT              = (1 << 2),
        CF_IMPROPERLY_REPRESENTED = (1 << 3),
    };

  public:
    /**
     * This enum is used to avoid std::dynamic_cast and std::dynamic_pointer_cast.
     */
    enum ChunkType {
        CT_EMPTY,
        CT_BITCOUNT,
        CT_BITS,
        CT_BYTECOUNT,
        CT_BYTES,
        CT_SLICE,
        CT_CPACKET,
        CT_SEQUENCE,
        CT_FIELDS
    };

    /**
     * This enum specifies bitmasks for the flags argument of various peek functions.
     */
    enum PeekFlag {
        PF_ALLOW_NULLPTR                = (1 << 0),
        PF_ALLOW_INCOMPLETE             = (1 << 1),
        PF_ALLOW_INCORRECT              = (1 << 2),
        PF_ALLOW_IMPROPERLY_REPRESENTED = (1 << 3),
        PF_ALLOW_SERIALIZATION          = (1 << 4),
    };

    class INET_API Iterator
    {
      protected:
        const bool isForward_;
        bit position;
        int index;

      public:
        Iterator(bit position) : isForward_(true), position(position), index(position == bit(0) ? 0 : -1) { CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }
        Iterator(byte position) : isForward_(true), position(position), index(position == bit(0) ? 0 : -1) { CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }
        explicit Iterator(bool isForward, bit position, int index) : isForward_(isForward), position(position), index(index) { CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }
        Iterator(const Iterator& other) : isForward_(other.isForward_), position(other.position), index(other.index) { CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }

        bool isCorrect() { return position >= bit(0) && index >= -1; }

        bool isForward() const { return isForward_; }
        bool isBackward() const { return !isForward_; }

        bit getPosition() const { return position; }
        void setPosition(bit position) { this->position = position; CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }

        int getIndex() const { return index; }
        void setIndex(int index) { this->index = index; CHUNK_CHECK_IMPLEMENTATION(isCorrect()); }
    };

    /**
     * Position and index are measured from the beginning.
     */
    class INET_API ForwardIterator : public Iterator
    {
      public:
        ForwardIterator(bit position) : Iterator(true, position, -1) { }
        ForwardIterator(byte position) : Iterator(true, position, -1) { }
        explicit ForwardIterator(bit position, int index) : Iterator(true, position, index) { }
        ForwardIterator& operator=(const ForwardIterator& other) { position = other.position; index = other.index; CHUNK_CHECK_IMPLEMENTATION(isCorrect()); return *this; }
    };

    /**
     * Position and index are measured from the end.
     */
    class INET_API BackwardIterator : public Iterator
    {
      public:
        BackwardIterator(bit position) : Iterator(false, position, -1) { }
        BackwardIterator(byte position) : Iterator(false, position, -1) { }
        explicit BackwardIterator(bit position, int index) : Iterator(false, position, index) { }
        BackwardIterator& operator=(const BackwardIterator& other) { position = other.position; index = other.index; CHUNK_CHECK_IMPLEMENTATION(isCorrect()); return *this; }
    };

  public:
    /**
     * Peeking some part into a chunk that requires automatic serialization
     * will throw an exception when implicit chunk serialization is disabled.
     */
    static bool enableImplicitChunkSerialization;

    static int nextId;

  protected:
    /**
     * The id is automatically assigned sequentially during construction.
     */
    int id;
    /**
     * The boolean chunk flags are merged into a single integer.
     */
    int flags;

  protected:
    virtual void handleChange() override;

    virtual int getBitsArraySize(); // only for class descriptor
    virtual int getBytesArraySize(); // only for class descriptor
    virtual const char *getBitsAsString(int index); // only for class descriptor
    virtual const char *getBytesAsString(int index); // only for class descriptor

    /**
     * Creates a new chunk of the given type that represents the designated part
     * of the provided chunk. The designated part starts at the provided offset
     * and has the provided length. This function isn't a constructor to allow
     * creating instances of message compiler generated field based chunk classes.
     */
    static Ptr<Chunk> convertChunk(const std::type_info& typeInfo, const Ptr<Chunk>& chunk, bit offset, bit length, int flags);

    typedef bool (*PeekPredicate)(const Ptr<Chunk>&);
    typedef Ptr<Chunk> (*PeekConverter)(const Ptr<Chunk>& chunk, const Chunk::Iterator& iterator, bit length, int flags);

    virtual Ptr<Chunk> peekUnchecked(PeekPredicate predicate, PeekConverter converter, const Iterator& iterator, bit length, int flags) const = 0;

    template <typename T>
    Ptr<T> peekConverted(const Iterator& iterator, bit length, int flags) const {
        CHUNK_CHECK_USAGE(iterator.isForward() || length != bit(-1), "chunk conversion using backward iterator with undefined length is invalid");
        auto offset = iterator.isForward() ? iterator.getPosition() : getChunkLength() - iterator.getPosition() - length;
        const auto& chunk = T::convertChunk(typeid(T), const_cast<Chunk *>(this)->shared_from_this(), offset, length, flags);
        chunk->markImmutable();
        return std::static_pointer_cast<T>(chunk);
    }

    template <typename T>
    Ptr<T> checkPeekResult(const Ptr<T>& chunk, int flags) const {
        if (chunk == nullptr) {
            if (!(flags & PF_ALLOW_NULLPTR))
                throw cRuntimeError("Returning an empty chunk is not allowed according to the flags: %x", flags);
        }
        else {
            if (chunk->isIncomplete() && !(flags & PF_ALLOW_INCOMPLETE))
                throw cRuntimeError("Returning an incomplete chunk is not allowed according to the flags: %x", flags);
            if (chunk->isIncorrect() && !(flags & PF_ALLOW_INCORRECT))
                throw cRuntimeError("Returning an incorrect chunk is not allowed according to the flags: %x", flags);
            if (chunk->isImproperlyRepresented() && !(flags & PF_ALLOW_IMPROPERLY_REPRESENTED))
                throw cRuntimeError("Returning an improperly represented chunk is not allowed according to the flags: %x", flags);
        }
        return chunk;
    }

  public:
    /** @name Constructors, destructors and duplication related functions */
    //@{
    Chunk();
    Chunk(const Chunk& other);

    /**
     * Returns a mutable copy of this chunk in a shared pointer.
     */
    virtual Ptr<Chunk> dupShared() const { return Ptr<Chunk>(static_cast<Chunk *>(dup())); };
    //@}

    /** @name Mutability related functions */
    //@{
    // NOTE: there is no markMutable() intentionally
    virtual bool isMutable() const { return !(flags & CF_IMMUTABLE); }
    virtual bool isImmutable() const { return flags & CF_IMMUTABLE; }
    virtual void markImmutable() { flags |= CF_IMMUTABLE; }
    void markMutableIfExclusivelyOwned() {
        // NOTE: one for external reference and one for local variable
        CHUNK_CHECK_USAGE(shared_from_this().use_count() == 2, "chunk is not exclusively owned");
        flags &= ~CF_IMMUTABLE;
    }
    //@}

    /** @name Completeness related functions */
    //@{
    // NOTE: there is no markComplete() intentionally
    virtual bool isComplete() const { return !(flags & CF_INCOMPLETE); }
    virtual bool isIncomplete() const { return flags & CF_INCOMPLETE; }
    virtual void markIncomplete() { flags |= CF_INCOMPLETE; }
    //@}

    /** @name Correctness related functions */
    //@{
    // NOTE: there is no markCorrect() intentionally
    virtual bool isCorrect() const { return !(flags & CF_INCORRECT); }
    virtual bool isIncorrect() const { return flags & CF_INCORRECT; }
    virtual void markIncorrect() { flags |= CF_INCORRECT; }
    //@}

    /** @name Proper representation related functions */
    //@{
    // NOTE: there is no markProperlyRepresented() intentionally
    virtual bool isProperlyRepresented() const { return !(flags & CF_IMPROPERLY_REPRESENTED); }
    virtual bool isImproperlyRepresented() const { return flags & CF_IMPROPERLY_REPRESENTED; }
    virtual void markImproperlyRepresented() { flags |= CF_IMPROPERLY_REPRESENTED; }
    //@}

    /** @name Iteration related functions */
    //@{
    virtual void moveIterator(Iterator& iterator, bit length) const;
    virtual void seekIterator(Iterator& iterator, bit position) const;
    //@}

    /** @name Inserting data related functions */
    //@{
    /**
     * Returns true if this chunk is capable of representing the result.
     */
    virtual bool canInsertAtBeginning(const Ptr<const Chunk>& chunk) const { return false; }

    /**
     * Returns true if this chunk is capable of representing the result.
     */
    virtual bool canInsertAtEnd(const Ptr<const Chunk>& chunk) const { return false; }

    /**
     * Inserts the provided chunk at the beginning of this chunk.
     */
    virtual void insertAtBeginning(const Ptr<const Chunk>& chunk) { throw cRuntimeError("Invalid operation"); }

    /**
     * Inserts the provided chunk at the end of this chunk.
     */
    virtual void insertAtEnd(const Ptr<const Chunk>& chunk) { throw cRuntimeError("Invalid operation"); }
    //@}

    /** @name Removing data related functions */
    //@{
    /**
     * Returns true if this chunk is capable of representing the result.
     */
    virtual bool canRemoveFromBeginning(bit length) const { return false; }

    /**
     * Returns true if this chunk is capable of representing the result.
     */
    virtual bool canRemoveFromEnd(bit length) const { return false; }

    /**
     * Removes the requested part from the beginning of this chunk and returns.
     */
    virtual void removeFromBeginning(bit length) { throw cRuntimeError("Invalid operation"); }

    /**
     * Removes the requested part from the end of this chunk.
     */
    virtual void removeFromEnd(bit length) { throw cRuntimeError("Invalid operation"); }
    //@}

    /** @name Chunk querying related functions */
    //@{
    /**
     * Returns the sequentially assigned id.
     */
    virtual int getChunkId() const { return id; }

    /**
     * Returns the type of this chunk as an enum member. This can be used to
     * avoid expensive std::dynamic_cast and std::dynamic_pointer_cast operators.
     */
    virtual ChunkType getChunkType() const = 0;

    /**
     * Returns the length of data represented by this chunk.
     */
    virtual bit getChunkLength() const = 0;

    /**
     * Returns the simplified representation of this chunk eliminating all potential
     * redundancies. This function may return a nullptr for emptry chunks.
     */
    virtual Ptr<Chunk> simplify() const {
        return peek(bit(0), getChunkLength());
    }

    /**
     * Returns the designated part of the data represented by this chunk in its
     * default representation. If the length is unspecified, then the length of
     * the result is chosen according to the internal representation. The result
     * is mutable iff the designated part is directly represented in this chunk
     * by a mutable chunk, otherwise the result is immutable.
     */
    virtual Ptr<Chunk> peek(const Iterator& iterator, bit length = bit(-1), int flags = 0) const;

    /**
     * Returns whether if the designated part of the data is available in the
     * requested representation.
     */
    template <typename T>
    bool has(const Iterator& iterator, bit length = bit(-1)) const {
        if (length != bit(-1) && getChunkLength() < iterator.getPosition() + length)
            return false;
        else {
            const auto& chunk = peek<T>(iterator, length, PF_ALLOW_NULLPTR | PF_ALLOW_INCOMPLETE);
            return chunk != nullptr && chunk->isComplete();
        }
    }

    /**
     * Returns the designated part of the data represented by this chunk in the
     * requested representation. If the length is unspecified, then the length of
     * the result is chosen according to the internal representation. The result
     * is mutable iff the designated part is directly represented in this chunk
     * by a mutable chunk, otherwise the result is immutable.
     */
    template <typename T>
    Ptr<T> peek(const Iterator& iterator, bit length = bit(-1), int flags = 0) const {
        const auto& predicate = [] (const Ptr<Chunk>& chunk) -> bool { return chunk == nullptr || std::dynamic_pointer_cast<T>(chunk); };
        const auto& converter = [] (const Ptr<Chunk>& chunk, const Iterator& iterator, bit length, int flags) -> Ptr<Chunk> { return chunk->peekConverted<T>(iterator, length, flags); };
        const auto& chunk = peekUnchecked(predicate, converter, iterator, length, flags);
        return checkPeekResult<T>(std::static_pointer_cast<T>(chunk), flags);
    }

    /**
     * Returns a human readable string representation of the data present in
     * this chunk.
     */
    virtual std::string str() const override;
    //@}

  public:
    /** @name Chunk serialization related functions */
    //@{
    /**
     * Serializes a chunk into the given stream. The bytes representing the
     * chunk is written at the current position of the stream up to its length.
     */
    static void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk, bit offset = bit(0), bit length = bit(-1));

    /**
     * Deserializes a chunk from the given stream. The returned chunk will be
     * an instance of the provided type. The chunk represents the bytes in the
     * stream starting from the current stream position up to the length
     * required by the deserializer of the chunk.
     */
    static Ptr<Chunk> deserialize(MemoryInputStream& stream, const std::type_info& typeInfo);
    //@}
};

inline std::ostream& operator<<(std::ostream& os, const Chunk *chunk) { return os << chunk->str(); }

inline std::ostream& operator<<(std::ostream& os, const Chunk& chunk) { return os << chunk.str(); }

} // namespace

#endif // #ifndef __INET_CHUNK_H_
