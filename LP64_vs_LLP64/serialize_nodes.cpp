#include <cstdint>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cassert>

// ---------------- In-memory node ----------------
struct Node
{
    int id;
    Node *next;
};

// ---------------- Endian helpers (C++11) ----------------
inline bool host_is_little_endian()
{
    union
    {
        uint16_t u16;
        unsigned char b[2];
    } v = {0x0100};
    return v.b[1] == 0x01; // 0x0001 in memory if LE; here we stored 0x0100, so LE -> [0x00,0x01]
}

inline std::uint32_t bswap32(std::uint32_t x)
{
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) | ((x & 0x00FF0000u) >> 8) | ((x & 0xFF000000u) >> 24);
}

inline void write_u32_le(std::ostream &os, std::uint32_t v)
{
    if (!host_is_little_endian())
        v = bswap32(v);
    os.write(reinterpret_cast<const char *>(&v), 4);
    if (!os)
        throw std::runtime_error("write_u32_le failed");
}

inline std::uint32_t read_u32_le(std::istream &is)
{
    std::uint32_t v = 0;
    is.read(reinterpret_cast<char *>(&v), 4);
    if (!is)
        throw std::runtime_error("read_u32_le failed");
    if (!host_is_little_endian())
        v = bswap32(v);
    return v;
}

// Signed 32-bit via unsigned transport
inline void write_s32_le(std::ostream &os, std::int32_t s)
{
    write_u32_le(os, static_cast<std::uint32_t>(s));
}
inline std::int32_t read_s32_le(std::istream &is)
{
    return static_cast<std::int32_t>(read_u32_le(is));
}

// ---------------- Wire format magic/version ----------------
static const unsigned char MAGIC[4] = {'N', 'D', 'L', 'S'};
static const std::uint32_t VERSION = 1;

// ---------------- Serializer ----------------
// Serializes the list reachable from 'head' (duplicates not removed).
void serialize_list(Node *head, std::ostream &os)
{
    // 1) Linearize nodes and assign indices
    std::vector<Node *> order;
    std::unordered_map<Node *, std::int32_t> index; // Node* -> index
    for (Node *p = head; p != nullptr; p = p->next)
    {
        index[p] = static_cast<std::int32_t>(order.size());
        order.push_back(p);
    }

    // 2) Header
    os.write(reinterpret_cast<const char *>(MAGIC), 4);
    if (!os)
        throw std::runtime_error("write magic failed");
    write_u32_le(os, VERSION);
    write_u32_le(os, static_cast<std::uint32_t>(order.size()));

    // 3) Body
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        Node *p = order[i];

        // id must fit s32 on the wire
        if (p->id < INT32_MIN || p->id > INT32_MAX)
            throw std::runtime_error("id out of s32 range for wire format");

        std::int32_t id = static_cast<std::int32_t>(p->id);
        std::int32_t nextIndex = -1;
        if (p->next)
        {
            // If next wasn't encountered in linearization, we’d need a full graph pass.
            // For a singly-linked list traversed from head, it will be encountered.
            std::unordered_map<Node *, std::int32_t>::const_iterator it = index.find(p->next);
            if (it == index.end())
            {
                // Fallback: assign new index? For strict lists this shouldn't happen.
                throw std::runtime_error("Encountered next pointer not in index map");
            }
            nextIndex = it->second;
        }

        write_s32_le(os, id);
        write_s32_le(os, nextIndex);
    }
}

// ---------------- Owning container for a deserialized list ----------------
struct List
{
    std::vector<Node> nodes; // owns storage
    Node *head() { return nodes.empty() ? nullptr : &nodes[0]; }
};

// ---------------- Deserializer ----------------
List deserialize_list(std::istream &is)
{
    // 1) Header
    unsigned char magic[4] = {0, 0, 0, 0};
    is.read(reinterpret_cast<char *>(magic), 4);
    if (!is)
        throw std::runtime_error("read magic failed");
    if (!(magic[0] == MAGIC[0] && magic[1] == MAGIC[1] && magic[2] == MAGIC[2] && magic[3] == MAGIC[3]))
        throw std::runtime_error("bad magic");

    std::uint32_t version = read_u32_le(is);
    if (version != VERSION)
        throw std::runtime_error("unsupported version");

    std::uint32_t count = read_u32_le(is);

    // 2) Reserve & temp next indices
    List out;
    out.nodes.resize(count);
    std::vector<std::int32_t> nextIdx(count, -1);

    // 3) Read nodes (ids & next indices)
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::int32_t id = read_s32_le(is);
        std::int32_t nxt = read_s32_le(is);
        out.nodes[i].id = static_cast<int>(id);
        out.nodes[i].next = nullptr; // link later
        nextIdx[i] = nxt;
    }

    // 4) Rebuild pointers
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (nextIdx[i] >= 0)
        {
            std::size_t j = static_cast<std::size_t>(nextIdx[i]);
            if (j >= out.nodes.size())
                throw std::runtime_error("next index out of range");
            out.nodes[i].next = &out.nodes[j];
        }
    }

    return out;
}

// ---------------- Demo / test ----------------
static void print_list(Node *head, const char *tag)
{
    std::cout << tag << ": ";
    for (const Node *p = head; p != nullptr; p = p->next)
    {
        std::cout << p->id << (p->next ? " -> " : "");
    }
    std::cout << "\n";
}

int main()
{
    // Build a simple list: 10 -> 20 -> 30
    Node n3{30, nullptr};
    Node n2{20, &n3};
    Node n1{10, &n2};

    print_list(&n1, "Original");

    // Serialize to file
    {
        std::ofstream ofs("list.bin", std::ios::binary);
        serialize_list(&n1, ofs);
        std::cout << "Wrote list.bin\n";
    }

    // Deserialize back
    List roundtrip;
    {
        std::ifstream ifs("list.bin", std::ios::binary);
        roundtrip = deserialize_list(ifs);
    }

    print_list(roundtrip.head(), "Deserialized");
    // Quick checks
    assert(roundtrip.nodes.size() == 3);
    assert(roundtrip.nodes[0].id == 10);
    assert(roundtrip.nodes[1].id == 20);
    assert(roundtrip.nodes[2].id == 30);
    assert(roundtrip.nodes[0].next == &roundtrip.nodes[1]);
    assert(roundtrip.nodes[1].next == &roundtrip.nodes[2]);
    assert(roundtrip.nodes[2].next == nullptr);

    std::cout << "Round-trip OK\n";
    return 0;
}