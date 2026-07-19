#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

constexpr std::uint64_t kDefaultSeed = 2025;
constexpr int kDefaultQueries = 300;
constexpr int kDefaultMinG = 4;
constexpr int kDefaultMaxG = 16;

struct Options
{
    fs::path source_root;
    fs::path data_root;
    std::string dataset = "all";
    std::uint64_t seed = kDefaultSeed;
    int query_count = kDefaultQueries;
    int min_g = kDefaultMinG;
    int max_g = kDefaultMaxG;
    bool force_graph = false;
};

class FastInput
{
public:
    explicit FastInput(const fs::path& path)
    {
        file_ = std::fopen(path.string().c_str(), "rb");
        if (file_ == nullptr)
        {
            throw std::runtime_error("Failed to open input file: " + path.string());
        }
        buffer_.resize(kBufferSize);
    }

    ~FastInput()
    {
        if (file_ != nullptr)
        {
            std::fclose(file_);
        }
    }

    bool ReadUInt64(std::uint64_t& value)
    {
        int c = NextNonSpace();
        if (c == EOF)
        {
            return false;
        }
        if (c < '0' || c > '9')
        {
            throw std::runtime_error("Expected a non-negative integer token");
        }

        value = 0;
        do
        {
            const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            {
                throw std::runtime_error("Integer token overflows uint64");
            }
            value = value * 10 + digit;
            c = GetChar();
        } while (c >= '0' && c <= '9');

        if (c != EOF && c > ' ')
        {
            throw std::runtime_error("Unexpected character in integer token");
        }
        return true;
    }

private:
    static constexpr std::size_t kBufferSize = 1U << 20;

    int GetChar()
    {
        if (position_ == size_)
        {
            size_ = std::fread(buffer_.data(), 1, kBufferSize, file_);
            position_ = 0;
            if (size_ == 0)
            {
                return EOF;
            }
        }
        return static_cast<unsigned char>(buffer_[position_++]);
    }

    int NextNonSpace()
    {
        int c = GetChar();
        while (c != EOF && c <= ' ')
        {
            c = GetChar();
        }
        return c;
    }

    std::FILE* file_ = nullptr;
    std::vector<char> buffer_;
    std::size_t position_ = 0;
    std::size_t size_ = 0;
};

class FastOutput
{
public:
    explicit FastOutput(const fs::path& path)
    {
        file_ = std::fopen(path.string().c_str(), "wb");
        if (file_ == nullptr)
        {
            throw std::runtime_error("Failed to open output file: " + path.string());
        }
        buffer_.resize(kBufferSize);
    }

    ~FastOutput()
    {
        try
        {
            Flush();
        }
        catch (...)
        {
        }
        if (file_ != nullptr)
        {
            std::fclose(file_);
        }
    }

    void WriteUInt64(std::uint64_t value)
    {
        char local[32];
        const auto result = std::to_chars(local, local + sizeof(local), value);
        if (result.ec != std::errc())
        {
            throw std::runtime_error("Failed to format integer");
        }
        Write(local, static_cast<std::size_t>(result.ptr - local));
    }

    void WriteChar(char c)
    {
        Write(&c, 1);
    }

    void Flush()
    {
        if (position_ == 0 || file_ == nullptr)
        {
            return;
        }
        if (std::fwrite(buffer_.data(), 1, position_, file_) != position_)
        {
            throw std::runtime_error("Failed while writing output file");
        }
        position_ = 0;
    }

private:
    static constexpr std::size_t kBufferSize = 1U << 20;

    void Write(const char* data, std::size_t length)
    {
        while (length > 0)
        {
            const std::size_t available = buffer_.size() - position_;
            if (available == 0)
            {
                Flush();
                continue;
            }
            const std::size_t amount = std::min(available, length);
            std::copy_n(data, amount, buffer_.data() + position_);
            data += amount;
            length -= amount;
            position_ += amount;
        }
    }

    std::FILE* file_ = nullptr;
    std::vector<char> buffer_;
    std::size_t position_ = 0;
};

class DisjointSet
{
public:
    explicit DisjointSet(std::uint32_t n) : parent_(n), size_(n, 1)
    {
        for (std::uint32_t i = 0; i < n; ++i)
        {
            parent_[i] = i;
        }
    }

    std::uint32_t Find(std::uint32_t x)
    {
        while (parent_[x] != x)
        {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    void Unite(std::uint32_t a, std::uint32_t b)
    {
        a = Find(a);
        b = Find(b);
        if (a == b)
        {
            return;
        }
        if (size_[a] < size_[b])
        {
            std::swap(a, b);
        }
        parent_[b] = a;
        size_[a] += size_[b];
    }

    void CompressAll()
    {
        for (std::uint32_t i = 0; i < parent_.size(); ++i)
        {
            parent_[i] = Find(i);
        }
        size_.clear();
        size_.shrink_to_fit();
    }

    std::uint32_t Component(std::uint32_t vertex) const
    {
        return parent_[vertex];
    }

private:
    std::vector<std::uint32_t> parent_;
    std::vector<std::uint32_t> size_;
};

struct GraphInfo
{
    std::uint32_t n = 0;
    std::uint64_t m = 0;
};

struct GroupIndex
{
    std::vector<std::uint64_t> group_offsets;
    std::vector<std::uint32_t> group_vertices;
    std::vector<std::uint64_t> vertex_offsets;
    std::vector<std::uint32_t> vertex_groups;

    std::size_t GroupCount() const
    {
        return group_offsets.empty() ? 0 : group_offsets.size() - 1;
    }
};

struct GenerationStats
{
    std::uint64_t derived_seed = 0;
    std::uint64_t insufficient_roots = 0;
    std::uint64_t infeasible_samples = 0;
    std::uint64_t depth_sum = 0;
    int min_depth = std::numeric_limits<int>::max();
    int max_depth = 0;
};

void ReplaceFile(const fs::path& temporary, const fs::path& destination)
{
    std::error_code error;
    fs::remove(destination, error);
    error.clear();
    fs::rename(temporary, destination, error);
    if (error)
    {
        throw std::runtime_error("Failed to install output file " + destination.string() + ": " + error.message());
    }
}

std::uint64_t ParseUInt64(const std::string& text, const std::string& option)
{
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
    {
        throw std::runtime_error("Invalid value for " + option + ": " + text);
    }
    return value;
}

int ParseInt(const std::string& text, const std::string& option)
{
    const std::uint64_t value = ParseUInt64(text, option);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("Value is too large for " + option + ": " + text);
    }
    return static_cast<int>(value);
}

Options ParseOptions(int argc, char** argv)
{
    if (argc < 3)
    {
        throw std::runtime_error(
            "Usage: gst_prepare_gpu4gst <source_root> <data_root> [dataset|all] "
            "[--seed N] [--queries N] [--min-g N] [--max-g N] [--force-graph]");
    }

    Options options;
    options.source_root = argv[1];
    options.data_root = argv[2];
    int i = 3;
    if (i < argc && std::string(argv[i]).rfind("--", 0) != 0)
    {
        options.dataset = argv[i++];
    }
    while (i < argc)
    {
        const std::string name = argv[i++];
        if (name == "--force-graph")
        {
            options.force_graph = true;
            continue;
        }
        if (i >= argc)
        {
            throw std::runtime_error("Missing value after " + name);
        }
        const std::string value = argv[i++];
        if (name == "--seed")
        {
            options.seed = ParseUInt64(value, name);
        }
        else if (name == "--queries")
        {
            options.query_count = ParseInt(value, name);
        }
        else if (name == "--min-g")
        {
            options.min_g = ParseInt(value, name);
        }
        else if (name == "--max-g")
        {
            options.max_g = ParseInt(value, name);
        }
        else
        {
            throw std::runtime_error("Unknown option: " + name);
        }
    }
    if (options.query_count <= 0 || options.min_g < 2 || options.max_g < options.min_g)
    {
        throw std::runtime_error("Require queries > 0 and 2 <= min-g <= max-g");
    }
    return options;
}

GraphInfo ReadGraphAndBuildComponents(const fs::path& source_path,
                                      const fs::path& destination_path,
                                      bool force_graph,
                                      DisjointSet*& components)
{
    FastInput input(source_path);
    std::uint64_t n64 = 0;
    std::uint64_t m = 0;
    if (!input.ReadUInt64(n64) || !input.ReadUInt64(m) || n64 == 0 ||
        n64 > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("Invalid graph header in " + source_path.string());
    }
    const std::uint32_t n = static_cast<std::uint32_t>(n64);
    auto dsu = std::make_unique<DisjointSet>(n);

    const bool write_graph = force_graph || !fs::exists(destination_path);
    const fs::path temporary_path = destination_path.string() + ".tmp";
    std::unique_ptr<FastOutput> output;
    if (write_graph)
    {
        output = std::make_unique<FastOutput>(temporary_path);
        output->WriteUInt64(n);
        output->WriteChar(' ');
        output->WriteUInt64(m);
        output->WriteChar('\n');
    }

    for (std::uint64_t edge = 0; edge < m; ++edge)
    {
        std::uint64_t u = 0;
        std::uint64_t v = 0;
        std::uint64_t weight = 0;
        if (!input.ReadUInt64(u) || !input.ReadUInt64(v) || !input.ReadUInt64(weight))
        {
            throw std::runtime_error("Graph ended at edge " + std::to_string(edge));
        }
        if (u >= n || v >= n)
        {
            throw std::runtime_error("Graph endpoint is out of range at edge " + std::to_string(edge));
        }
        dsu->Unite(static_cast<std::uint32_t>(u), static_cast<std::uint32_t>(v));
        if (output)
        {
            output->WriteUInt64(u + 1);
            output->WriteChar(' ');
            output->WriteUInt64(v + 1);
            output->WriteChar(' ');
            output->WriteUInt64(weight);
            output->WriteChar('\n');
        }
    }
    std::uint64_t trailing = 0;
    if (input.ReadUInt64(trailing))
    {
        throw std::runtime_error("Graph has tokens after the declared edge count");
    }
    dsu->CompressAll();

    if (output)
    {
        output->Flush();
        output.reset();
        ReplaceFile(temporary_path, destination_path);
    }
    else
    {
        std::ifstream existing(destination_path);
        std::uint64_t existing_n = 0;
        std::uint64_t existing_m = 0;
        existing >> existing_n >> existing_m;
        if (!existing || existing_n != n || existing_m != m)
        {
            throw std::runtime_error("Existing graph.txt header does not match the source; use --force-graph");
        }
    }

    components = dsu.release();
    return {n, m};
}

bool ParseGroupLine(const std::string& line,
                    std::uint32_t expected_group_id,
                    std::uint32_t vertex_count,
                    std::vector<std::uint32_t>& vertices,
                    std::vector<std::uint32_t>& vertex_group_counts,
                    std::vector<std::uint32_t>& vertex_seen_in_group)
{
    if (line.empty())
    {
        return false;
    }
    if (line[0] != 'g')
    {
        throw std::runtime_error("Group line does not start with 'g'");
    }
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos)
    {
        throw std::runtime_error("Group line is missing ':'");
    }

    std::uint32_t group_id = 0;
    const auto id_result = std::from_chars(line.data() + 1, line.data() + colon, group_id);
    if (id_result.ec != std::errc() || id_result.ptr != line.data() + colon || group_id != expected_group_id)
    {
        throw std::runtime_error("Expected group g" + std::to_string(expected_group_id));
    }

    const char* current = line.data() + colon + 1;
    const char* end = line.data() + line.size();
    std::size_t added = 0;
    while (current < end)
    {
        while (current < end && *current <= ' ')
        {
            ++current;
        }
        if (current == end)
        {
            break;
        }
        std::uint32_t vertex = 0;
        const auto result = std::from_chars(current, end, vertex);
        if (result.ec != std::errc() || result.ptr == current || vertex >= vertex_count)
        {
            throw std::runtime_error("Invalid vertex in group g" + std::to_string(group_id));
        }
        if (vertex_seen_in_group[vertex] == group_id)
        {
            throw std::runtime_error("Duplicate vertex in group g" + std::to_string(group_id));
        }
        vertex_seen_in_group[vertex] = group_id;
        current = result.ptr;
        vertices.push_back(vertex);
        ++vertex_group_counts[vertex];
        ++added;
    }
    if (added == 0)
    {
        throw std::runtime_error("Empty candidate group g" + std::to_string(group_id));
    }
    return true;
}

GroupIndex LoadGroups(const fs::path& path, std::uint32_t vertex_count)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Failed to open group file: " + path.string());
    }

    GroupIndex index;
    index.group_offsets.push_back(0);
    std::vector<std::uint32_t> vertex_group_counts(vertex_count, 0);
    std::vector<std::uint32_t> vertex_seen_in_group(vertex_count, 0);
    std::string line;
    std::uint32_t expected_group_id = 1;
    while (std::getline(input, line))
    {
        if (!ParseGroupLine(line, expected_group_id, vertex_count,
                            index.group_vertices, vertex_group_counts, vertex_seen_in_group))
        {
            continue;
        }
        index.group_offsets.push_back(index.group_vertices.size());
        ++expected_group_id;
    }
    if (index.GroupCount() == 0)
    {
        throw std::runtime_error("No candidate groups in " + path.string());
    }

    index.vertex_offsets.resize(static_cast<std::size_t>(vertex_count) + 1, 0);
    for (std::uint32_t vertex = 0; vertex < vertex_count; ++vertex)
    {
        index.vertex_offsets[vertex + 1] = index.vertex_offsets[vertex] + vertex_group_counts[vertex];
    }
    index.vertex_groups.resize(index.group_vertices.size());
    std::fill(vertex_group_counts.begin(), vertex_group_counts.end(), 0);
    for (std::uint32_t group = 0; group < index.GroupCount(); ++group)
    {
        for (std::uint64_t p = index.group_offsets[group]; p < index.group_offsets[group + 1]; ++p)
        {
            const std::uint32_t vertex = index.group_vertices[p];
            const std::uint64_t destination = index.vertex_offsets[vertex] + vertex_group_counts[vertex]++;
            index.vertex_groups[destination] = group;
        }
    }
    return index;
}

std::uint64_t Fnv1a(const std::string& text)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : text)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t SplitMix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t UniformIndex(std::mt19937_64& random, std::uint64_t bound)
{
    if (bound == 0)
    {
        throw std::runtime_error("UniformIndex called with bound zero");
    }
    const std::uint64_t threshold = static_cast<std::uint64_t>(-bound) % bound;
    while (true)
    {
        const std::uint64_t value = random();
        if (value >= threshold)
        {
            return value % bound;
        }
    }
}

template <class T>
void Shuffle(std::vector<T>& values, std::mt19937_64& random)
{
    for (std::size_t i = values.size(); i > 1; --i)
    {
        const std::size_t j = static_cast<std::size_t>(UniformIndex(random, i));
        std::swap(values[i - 1], values[j]);
    }
}

std::vector<std::uint32_t> NearbyGroups(const GroupIndex& index,
                                        std::uint32_t root,
                                        std::size_t required,
                                        std::vector<std::uint32_t>& seen,
                                        std::uint32_t& stamp,
                                        int& depth)
{
    ++stamp;
    if (stamp == 0)
    {
        std::fill(seen.begin(), seen.end(), 0);
        stamp = 1;
    }

    std::vector<std::uint32_t> queue;
    queue.push_back(root);
    seen[root] = stamp;
    std::size_t level_begin = 0;
    std::size_t level_end = 1;
    depth = 0;
    while (queue.size() - 1 < required && level_begin < level_end)
    {
        for (std::size_t i = level_begin; i < level_end; ++i)
        {
            const std::uint32_t group = queue[i];
            for (std::uint64_t gp = index.group_offsets[group]; gp < index.group_offsets[group + 1]; ++gp)
            {
                const std::uint32_t vertex = index.group_vertices[gp];
                for (std::uint64_t vp = index.vertex_offsets[vertex]; vp < index.vertex_offsets[vertex + 1]; ++vp)
                {
                    const std::uint32_t neighbor = index.vertex_groups[vp];
                    if (seen[neighbor] != stamp)
                    {
                        seen[neighbor] = stamp;
                        queue.push_back(neighbor);
                    }
                }
            }
        }
        level_begin = level_end;
        level_end = queue.size();
        ++depth;
    }
    queue.erase(queue.begin());
    return queue;
}

std::vector<std::uint32_t> UniformSample(const std::vector<std::uint32_t>& candidates,
                                         std::size_t count,
                                         std::mt19937_64& random)
{
    if (candidates.size() < count)
    {
        return {};
    }
    std::vector<std::uint32_t> sample(candidates.begin(), candidates.begin() + count);
    for (std::size_t i = count; i < candidates.size(); ++i)
    {
        const std::size_t position = static_cast<std::size_t>(UniformIndex(random, i + 1));
        if (position < count)
        {
            sample[position] = candidates[i];
        }
    }
    return sample;
}

class ComponentCache
{
public:
    ComponentCache(const GroupIndex& index, const DisjointSet& components)
        : index_(index), components_(components)
    {
    }

    const std::vector<std::uint32_t>& Get(std::uint32_t group)
    {
        auto found = cache_.find(group);
        if (found != cache_.end())
        {
            return found->second;
        }
        std::vector<std::uint32_t> values;
        values.reserve(static_cast<std::size_t>(index_.group_offsets[group + 1] - index_.group_offsets[group]));
        for (std::uint64_t p = index_.group_offsets[group]; p < index_.group_offsets[group + 1]; ++p)
        {
            values.push_back(components_.Component(index_.group_vertices[p]));
        }
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        return cache_.emplace(group, std::move(values)).first->second;
    }

private:
    const GroupIndex& index_;
    const DisjointSet& components_;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> cache_;
};

bool IsFeasible(const std::vector<std::uint32_t>& groups, ComponentCache& cache)
{
    std::vector<std::uint32_t> common = cache.Get(groups.front());
    for (std::size_t i = 1; i < groups.size() && !common.empty(); ++i)
    {
        const auto& current = cache.Get(groups[i]);
        std::vector<std::uint32_t> intersection;
        intersection.reserve(std::min(common.size(), current.size()));
        std::set_intersection(common.begin(), common.end(), current.begin(), current.end(),
                              std::back_inserter(intersection));
        common.swap(intersection);
    }
    return !common.empty();
}

void WriteQueryFile(const fs::path& path,
                    const std::vector<std::vector<std::uint32_t>>& queries,
                    const GroupIndex& index)
{
    const fs::path temporary = path.string() + ".tmp";
    {
        FastOutput output(temporary);
        output.WriteUInt64(queries.size());
        output.WriteChar('\n');
        for (const auto& query : queries)
        {
            output.WriteUInt64(query.size());
            output.WriteChar('\n');
            for (std::uint32_t group : query)
            {
                const std::uint64_t size = index.group_offsets[group + 1] - index.group_offsets[group];
                output.WriteUInt64(size);
                for (std::uint64_t p = index.group_offsets[group]; p < index.group_offsets[group + 1]; ++p)
                {
                    output.WriteChar(' ');
                    output.WriteUInt64(static_cast<std::uint64_t>(index.group_vertices[p]) + 1);
                }
                output.WriteChar('\n');
            }
        }
        output.Flush();
    }
    ReplaceFile(temporary, path);
}

void WriteGroupIds(const fs::path& path,
                   const std::string& protocol,
                   std::uint64_t base_seed,
                   std::uint64_t derived_seed,
                   const std::vector<std::vector<std::uint32_t>>& queries)
{
    const fs::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary);
    if (!output)
    {
        throw std::runtime_error("Failed to open " + temporary.string());
    }
    output << "# protocol=" << protocol << '\n';
    output << "# group_ids=1-based vertex_ids_in_query_file=1-based\n";
    if (protocol == "related-group-bfs")
    {
        output << "# base_seed=" << base_seed << " derived_seed=" << derived_seed << '\n';
    }
    output << "# queries=" << queries.size() << '\n';
    for (const auto& query : queries)
    {
        for (std::size_t i = 0; i < query.size(); ++i)
        {
            if (i != 0)
            {
                output << ' ';
            }
            output << query[i] + 1;
        }
        output << '\n';
    }
    output.close();
    ReplaceFile(temporary, path);
}

std::vector<std::vector<std::uint32_t>> ReadAuthorQueries(const fs::path& path,
                                                          int group_count,
                                                          int query_count,
                                                          std::size_t candidate_group_count)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Failed to open author query file: " + path.string());
    }
    std::vector<std::vector<std::uint32_t>> queries;
    std::string line;
    while (queries.size() < static_cast<std::size_t>(query_count) && std::getline(input, line))
    {
        std::istringstream row(line);
        std::vector<std::uint32_t> query;
        std::uint64_t group_id = 0;
        while (row >> group_id)
        {
            // Author commit 716a19c maps .g label gN to group_graph[N - 1],
            // then reads each CSV integer unchanged before indexing group_graph.
            if (group_id >= candidate_group_count)
            {
                throw std::runtime_error("Author query group id is out of range in " + path.string());
            }
            query.push_back(static_cast<std::uint32_t>(group_id));
        }
        if (query.empty())
        {
            continue;
        }
        if (query.size() != static_cast<std::size_t>(group_count))
        {
            throw std::runtime_error("Author query has the wrong group count in " + path.string());
        }
        std::vector<std::uint32_t> unique = query;
        std::sort(unique.begin(), unique.end());
        if (std::unique(unique.begin(), unique.end()) != unique.end())
        {
            throw std::runtime_error("Author query repeats a group in " + path.string());
        }
        queries.push_back(std::move(query));
    }
    if (queries.size() != static_cast<std::size_t>(query_count))
    {
        throw std::runtime_error("Author query file contains fewer than " + std::to_string(query_count) + " rows");
    }
    return queries;
}

std::vector<std::vector<std::uint32_t>> GenerateQueries(const std::string& dataset,
                                                        int group_count,
                                                        int query_count,
                                                        std::uint64_t base_seed,
                                                        const GroupIndex& index,
                                                        ComponentCache& component_cache,
                                                        GenerationStats& stats)
{
    stats.derived_seed = SplitMix64(base_seed ^ Fnv1a(dataset) ^
                                    (static_cast<std::uint64_t>(group_count) * 0x9e3779b97f4a7c15ULL));
    std::mt19937_64 random(stats.derived_seed);
    std::vector<std::uint32_t> seen(index.GroupCount(), 0);
    std::vector<unsigned char> insufficient(index.GroupCount(), 0);
    std::uint32_t stamp = 0;
    std::size_t insufficient_count = 0;
    std::vector<std::vector<std::uint32_t>> queries;
    queries.reserve(query_count);

    while (queries.size() < static_cast<std::size_t>(query_count))
    {
        if (insufficient_count == index.GroupCount())
        {
            throw std::runtime_error("No root group can reach " + std::to_string(group_count - 1) + " nearby groups");
        }
        const std::uint32_t root = static_cast<std::uint32_t>(UniformIndex(random, index.GroupCount()));
        if (insufficient[root] != 0)
        {
            continue;
        }
        int depth = 0;
        const auto nearby = NearbyGroups(index, root, static_cast<std::size_t>(group_count - 1), seen, stamp, depth);
        if (nearby.size() < static_cast<std::size_t>(group_count - 1))
        {
            insufficient[root] = 1;
            ++insufficient_count;
            ++stats.insufficient_roots;
            continue;
        }

        auto query = UniformSample(nearby, static_cast<std::size_t>(group_count - 1), random);
        query.push_back(root);
        if (!IsFeasible(query, component_cache))
        {
            ++stats.infeasible_samples;
            continue;
        }
        Shuffle(query, random);
        queries.push_back(std::move(query));
        stats.depth_sum += static_cast<std::uint64_t>(depth);
        stats.min_depth = std::min(stats.min_depth, depth);
        stats.max_depth = std::max(stats.max_depth, depth);
    }
    return queries;
}

void WriteManifest(const fs::path& path,
                   const std::string& dataset,
                   const GraphInfo& graph,
                   const GroupIndex& index,
                   const Options& options,
                   const std::vector<std::pair<int, GenerationStats>>& generated)
{
    const fs::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary);
    if (!output)
    {
        throw std::runtime_error("Failed to open manifest: " + temporary.string());
    }
    output << "{\n";
    output << "  \"dataset\": \"GPU4GST_" << dataset << "\",\n";
    output << "  \"source_prefix\": \"" << dataset << "\",\n";
    output << "  \"vertices\": " << graph.n << ",\n";
    output << "  \"edges\": " << graph.m << ",\n";
    output << "  \"candidate_groups\": " << index.GroupCount() << ",\n";
    output << "  \"vertex_group_memberships\": " << index.group_vertices.size() << ",\n";
    output << "  \"generated_query_protocol\": \"related-group-bfs\",\n";
    output << "  \"base_seed\": " << options.seed << ",\n";
    output << "  \"queries_per_g\": " << options.query_count << ",\n";
    output << "  \"generated\": [\n";
    for (std::size_t i = 0; i < generated.size(); ++i)
    {
        const int g = generated[i].first;
        const auto& stats = generated[i].second;
        output << "    {\"g\": " << g
               << ", \"seed\": " << stats.derived_seed
               << ", \"min_bfs_depth\": " << stats.min_depth
               << ", \"max_bfs_depth\": " << stats.max_depth
               << ", \"mean_bfs_depth\": "
               << static_cast<double>(stats.depth_sum) / options.query_count
               << ", \"insufficient_root_rejections\": " << stats.insufficient_roots
               << ", \"infeasible_query_rejections\": " << stats.infeasible_samples << "}";
        output << (i + 1 == generated.size() ? "\n" : ",\n");
    }
    output << "  ],\n";
    output << "  \"author_query_files\": [\"query_author_g3.txt\", \"query_author_g5.txt\", \"query_author_g7.txt\"]\n";
    output << "}\n";
    output.close();
    ReplaceFile(temporary, path);
}

void PrepareDataset(const std::string& dataset, const Options& options)
{
    const fs::path source_graph = options.source_root / (dataset + ".in");
    const fs::path source_groups = options.source_root / (dataset + ".g");
    if (!fs::exists(source_graph) || !fs::exists(source_groups))
    {
        throw std::runtime_error("Missing source files for dataset " + dataset);
    }

    const fs::path destination = options.data_root / ("GPU4GST_" + dataset);
    fs::create_directories(destination);
    std::cout << "[" << dataset << "] graph and connected components" << std::endl;
    DisjointSet* raw_components = nullptr;
    const GraphInfo graph = ReadGraphAndBuildComponents(
        source_graph, destination / "graph.txt", options.force_graph, raw_components);
    std::unique_ptr<DisjointSet> components(raw_components);

    std::cout << "[" << dataset << "] candidate-group index" << std::endl;
    const GroupIndex groups = LoadGroups(source_groups, graph.n);
    if (groups.GroupCount() < static_cast<std::size_t>(options.max_g))
    {
        throw std::runtime_error("Dataset has fewer candidate groups than max-g");
    }
    ComponentCache component_cache(groups, *components);

    for (int g : {3, 5, 7})
    {
        const fs::path source_query = options.source_root / (dataset + std::to_string(g) + ".csv");
        const auto queries = ReadAuthorQueries(source_query, g, kDefaultQueries, groups.GroupCount());
        for (const auto& query : queries)
        {
            if (!IsFeasible(query, component_cache))
            {
                throw std::runtime_error("Author query file contains an infeasible query: " + source_query.string());
            }
        }
        WriteQueryFile(destination / ("query_author_g" + std::to_string(g) + ".txt"), queries, groups);
        WriteGroupIds(destination / ("query_author_g" + std::to_string(g) + ".group_ids.txt"),
                      "author-csv", 0, 0, queries);
    }

    std::vector<std::pair<int, GenerationStats>> generated;
    for (int g = options.min_g; g <= options.max_g; ++g)
    {
        std::cout << "[" << dataset << "] generate g=" << g << std::endl;
        GenerationStats stats;
        const auto queries = GenerateQueries(dataset, g, options.query_count, options.seed,
                                             groups, component_cache, stats);
        WriteQueryFile(destination / ("query_g" + std::to_string(g) + ".txt"), queries, groups);
        WriteGroupIds(destination / ("query_g" + std::to_string(g) + ".group_ids.txt"),
                      "related-group-bfs", options.seed, stats.derived_seed, queries);
        generated.push_back({g, stats});
    }
    WriteManifest(destination / "dataset_manifest.json", dataset, graph, groups, options, generated);
    std::cout << "[" << dataset << "] done: n=" << graph.n << " m=" << graph.m
              << " groups=" << groups.GroupCount() << " memberships=" << groups.group_vertices.size()
              << std::endl;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = ParseOptions(argc, argv);
        const std::vector<std::string> datasets = {
            "Musae", "Twitch", "Github", "Youtube", "DBLP", "Orkut", "LiveJournal", "Reddit"};
        if (options.dataset == "all")
        {
            for (const auto& dataset : datasets)
            {
                PrepareDataset(dataset, options);
            }
        }
        else
        {
            const auto found = std::find(datasets.begin(), datasets.end(), options.dataset);
            if (found == datasets.end())
            {
                throw std::runtime_error("Unknown dataset prefix: " + options.dataset);
            }
            PrepareDataset(*found, options);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "gpu4gst preparation failed: " << error.what() << '\n';
        return 1;
    }
}
