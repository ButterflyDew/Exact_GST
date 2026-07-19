#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
struct Edge
{
    int u = 0, v = 0, w = 0;
};

struct Instance
{
    int n = 0;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> groups;
};

std::string Quote(const fs::path& path)
{
    std::string s = path.string();
    std::string out = "\"";
    for (char c : s)
    {
        if (c == '"')
            out += "\\\"";
        else
            out += c;
    }
    out += '"';
    return out;
}

void AddEdge(Instance& inst, int u, int v, int w)
{
    inst.edges.push_back({u, v, w});
}

Instance RandomInstance(std::mt19937_64& rng, int n, int g)
{
    Instance inst;
    inst.n = n;
    std::uniform_int_distribution<int> weight(1, 30);
    for (int v = 2; v <= n; ++v)
    {
        std::uniform_int_distribution<int> parent(1, v - 1);
        AddEdge(inst, v, parent(rng), weight(rng));
    }
    std::bernoulli_distribution extra(0.32);
    for (int u = 1; u <= n; ++u)
        for (int v = u + 1; v <= n; ++v)
            if (extra(rng))
                AddEdge(inst, u, v, weight(rng));

    inst.groups.resize(g);
    std::uniform_int_distribution<int> vertex(1, n);
    std::uniform_int_distribution<int> group_size(1, std::min(3, n));
    for (auto& group : inst.groups)
    {
        int k = group_size(rng);
        while (static_cast<int>(group.size()) < k)
        {
            int v = vertex(rng);
            if (std::find(group.begin(), group.end(), v) == group.end())
                group.push_back(v);
        }
        std::sort(group.begin(), group.end());
    }
    return inst;
}

void WriteInstance(const fs::path& data_root, const Instance& inst)
{
    fs::path graph_dir = data_root / "random";
    fs::create_directories(graph_dir);

    std::ofstream graph(graph_dir / "graph.txt");
    graph << inst.n << ' ' << inst.edges.size() << '\n';
    for (const auto& e : inst.edges)
        graph << e.u << ' ' << e.v << ' ' << e.w << '\n';

    std::ofstream query(graph_dir / "query.txt");
    query << 1 << '\n';
    query << inst.groups.size() << '\n';
    for (const auto& group : inst.groups)
    {
        query << group.size();
        for (int v : group)
            query << ' ' << v;
        query << '\n';
    }
}

bool ReadLastWeight(const fs::path& file, double& weight)
{
    std::ifstream in(file);
    if (!in)
        return false;
    std::string line;
    bool found = false;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream iss(line);
        double time = 0.0;
        if (iss >> time >> weight)
            found = true;
    }
    return found;
}

int RunMain(const fs::path& exe,
            const fs::path& data_root,
            const fs::path& result_root,
            const std::vector<std::string>& extra_arguments = {})
{
    std::string cmd = Quote(exe) + " random " + Quote(result_root) +
                      " query.txt " + Quote(data_root) + " 1 1";
    for (const std::string& argument : extra_arguments)
        cmd += " \"" + argument + "\"";
#if defined(_WIN32)
    cmd += " > NUL 2> NUL";
    cmd = "\"" + cmd + "\"";
#else
    cmd += " >/dev/null 2>/dev/null";
#endif
    return std::system(cmd.c_str());
}

void DumpInstance(const Instance& inst)
{
    std::cout << "GRAPH " << inst.n << ' ' << inst.edges.size() << '\n';
    for (const auto& e : inst.edges)
        std::cout << e.u << ' ' << e.v << ' ' << e.w << '\n';
    std::cout << "QUERY " << inst.groups.size() << '\n';
    for (const auto& group : inst.groups)
    {
        std::cout << group.size();
        for (int v : group)
            std::cout << ' ' << v;
        std::cout << '\n';
    }
}

int Usage()
{
    std::cerr
        << "Usage:\n"
        << "  gst_random_compare <method_exe> <dpbf_exe> <method_name>"
        << " [seed=1] [iterations=1000] [min_n=4] [max_n=10]"
        << " [min_g=2] [max_g=8] [work_root=.random_compare_tmp] [keep=0]"
        << " [method_extra_args...]\n";
    return 2;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
        return Usage();

    fs::path method_exe = fs::absolute(argv[1]);
    fs::path dpbf_exe = fs::absolute(argv[2]);
    std::string method_name = argv[3];
    std::uint64_t seed = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1;
    int iterations = argc > 5 ? std::atoi(argv[5]) : 1000;
    int min_n = argc > 6 ? std::atoi(argv[6]) : 4;
    int max_n = argc > 7 ? std::atoi(argv[7]) : 10;
    int min_g = argc > 8 ? std::atoi(argv[8]) : 2;
    int max_g = argc > 9 ? std::atoi(argv[9]) : 8;
    fs::path work_root = fs::absolute(argc > 10 ? fs::path(argv[10]) : fs::path(".random_compare_tmp"));
    bool keep = argc > 11 && std::atoi(argv[11]) != 0;
    std::vector<std::string> method_arguments;
    for (int index = 12; index < argc; ++index)
        method_arguments.emplace_back(argv[index]);

    if (min_n > max_n || min_g > max_g || iterations < 0)
        return Usage();

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> n_dist(min_n, max_n);
    std::uniform_int_distribution<int> g_dist(min_g, max_g);

    fs::create_directories(work_root);

    for (int it = 1; it <= iterations; ++it)
    {
        fs::path case_root = work_root / ("case_" + std::to_string(it));
        fs::path data_root = case_root / "data";
        fs::path dpbf_root = case_root / "dpbf_result";
        fs::path method_root = case_root / "method_result";
        Instance inst = RandomInstance(rng, n_dist(rng), g_dist(rng));
        std::error_code ec;
        fs::remove_all(case_root, ec);
        WriteInstance(data_root, inst);

        int dpbf_code = RunMain(dpbf_exe, data_root, dpbf_root);
        int method_code = RunMain(method_exe, data_root, method_root, method_arguments);
        if (dpbf_code != 0 || method_code != 0)
        {
            std::cout << "RUN_FAILED seed=" << seed << " iteration=" << it
                      << " dpbf_code=" << dpbf_code
                      << " method_code=" << method_code << '\n';
            DumpInstance(inst);
            return 3;
        }

        double exact = 0.0, got = 0.0;
        fs::path exact_file = dpbf_root / "random" / "DPBF" / "default" / "weights.txt";
        fs::path got_file = method_root / "random" / method_name / "default" / "weights.txt";
        if (!ReadLastWeight(exact_file, exact) || !ReadLastWeight(got_file, got))
        {
            std::cout << "OUTPUT_MISSING seed=" << seed << " iteration=" << it << '\n'
                      << "exact_file=" << exact_file.string() << '\n'
                      << "method_file=" << got_file.string() << '\n';
            DumpInstance(inst);
            return 4;
        }
        if (std::fabs(exact - got) > 1e-6)
        {
            std::cout << std::setprecision(17)
                      << "MISMATCH seed=" << seed << " iteration=" << it
                      << " exact=" << exact << " got=" << got << '\n';
            DumpInstance(inst);
            return 1;
        }
        if (it % 100 == 0)
            std::cout << "ok " << it << '\n';

        if (!keep)
        {
            std::error_code cleanup_ec;
            fs::remove_all(case_root, cleanup_ec);
        }
    }

    std::cout << "ALL_OK seed=" << seed << " iterations=" << iterations << '\n';
    return 0;
}
