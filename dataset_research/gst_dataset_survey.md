# GST 可新增数据集调研

本文整理用于当前 GST 精确算法框架的新增数据集候选。这里的 `g` 表示一次 GST 查询中的组数，也就是 keyword / label / skill / PoI 的数量；`f` 表示每个组的候选顶点数，很多论文称为 label frequency，若原文未直接给出，则说明如何由原始数据生成或统计。

当前仓库已有 `Toronto`、`MovieLens`、`DBLP`、`DBpedia`、`LinkedMDB`。下面优先记录不只是强化仓库现有五个数据集的候选来源。

## 1. Amazon Co-Purchase / PGST

- 论文：Shuang Yang, Yahui Sun, Jiesong Liu, Xiaokui Xiao, Rong-Hua Li, Zhewei Wei. "Approximating Probabilistic Group Steiner Trees in Graphs." PVLDB 2022.
- 论文页面：http://www.vldb.org/pvldb/volumes/16/paper/Approximating%20Probabilistic%20Group%20Steiner%20Trees%20in%20Graphs
- 数据/代码：https://github.com/rucdatascience/PGST
- 原始场景：电商共购网络。顶点是 Amazon item，边表示两个 item 被用户同时购买。
- 图类型：无向或可无向化的 item co-purchase graph；边可设单位权，或用共购强度转距离权；顶点带 keyword/PoI 和平均评分。
- 图规模：548,552 item vertices；987,942 co-purchase links；25,958 keywords。
- group 来源：每个 keyword/PoI 对应一组包含该 keyword 的 item。
- `g`：PGST 论文中的 `|Gamma|` 是 PoI 数。公开 README/PVLDB 页面未列出具体实验 `g` 取值；用于本仓库时建议生成 `g in {4, 6, 8, 10, 12, 16, 20}`。
- `f`：原文未直接列实验 `f`；可由 `amazon_items.txt` 倒排统计每个 keyword 命中的 item 数。若按本仓库已有习惯，可筛选 keyword frequency 后生成 `f` 分档，例如 `100/200/400/600/800`。
- 适配说明：PGST 是概率组斯坦纳树，顶点对 PoI 有概率；若转普通 GST，可把概率大于阈值或含 keyword 的 item 作为候选顶点。该数据集能补足电商推荐/共购场景。

## 2. Mondial 地理数据库

- 相关论文：Konstantin Golenberg and Yehoshua Sagiv. "A Practically Efficient Algorithm for Generating Answers to Keyword Search over Data Graphs." ICDT 2016 / arXiv 1512.06635.
- 论文页面：https://ar5iv.labs.arxiv.org/html/1512.06635
- 数据来源：https://www.dbis.informatik.uni-goettingen.de/Mondial/
- 原始场景：地理与政治知识库，包括国家、城市、省份、河流、山脉、组织、语言、宗教等实体。
- 图类型：关系数据库或 RDF triplification 转 data graph；节点是实体/关系/tuple，边来自外键、引用或 RDF triple。通常是有向图，适配当前框架时可无向化。
- 图规模：GTF 论文报告 Mondial data graph 为 21K nodes、86K edges，平均度 4.04；该统计不含 keyword nodes 及其 incident edges。
- group 来源：keyword search。每个查询 keyword 命中的节点集合构成一个 group。
- `g`：GTF 实验手工构造 `2..10` keywords，每个 query size 有 4 个 queries。RDF benchmark 论文中也使用 Mondial 的 24 个复杂 keyword queries。
- `f`：原文未给整体 `f` 表；每个 keyword 的候选数由倒排索引决定。RDF benchmark 示例中，查询的 selected seeds 通常为 2-5，例如 `niger country` 为 4 seeds，`haiti religion` 为 2 seeds，`poland cape verde organization` 为 5 seeds。用于本仓库时应从文本索引统计每组候选节点数。
- 适配说明：规模小到中等、结构高连通，适合作为精确 GST 算法的新增 sanity / stress 数据集。

## 3. IMDB 关系图

- 相关论文 1：STAR, "STAR: Steiner-Tree Approximation in Relationship Graphs." ICDE 2009.
- STAR PDF：https://resources.mpi-inf.mpg.de/yago-naga/naga/download/ICDEResearchLong09_264.pdf
- 相关论文 2：Kargar, An, Zihayat. "Efficient Bi-objective Team Formation in Social Networks." ECML PKDD 2012.
- ECML PKDD PDF：https://www.eecs.yorku.ca/~aan/research/paper/pkdd12.pdf
- 相关论文 3：BANKS/BLINKS/EMBANKS 系列 keyword search。
- 原始场景：电影数据库。实体包括电影、演员、导演、角色、年份等；边表示参演、导演、引用、同片合作等关系。
- 图类型：异构信息网络、关系数据库图或 team-formation expert graph；通常有向或异构，适配时可无向化。
- 图规模：
  - STAR 子图：30,000 nodes、80,000 edges；边权为随机 `0..1`，因为原始 DBLP/IMDB 无边权。
  - ECML PKDD team graph：6,784 nodes、35,875 edges；节点是 actor/expert，边权按共同电影集合的 Jaccard 距离。
  - BANKS/EMBANKS 关系图版本：约 1.740M nodes、3.968M directed edges。
  - Temporal keyword search 版本：145K vertices、397K edges、17.3K time instants。
- group 来源：
  - 关系图 keyword search：每个 keyword 命中的实体/tuple 是一个 group。
  - team formation：每个 movie genre / skill 是一个 group，候选顶点是具备该 skill 的 actor/expert。
- `g`：
  - STAR：3、5、7 terminals，每档 60 queries；这是普通 Steiner tree，可扩展为 singleton-group GST。
  - ECML PKDD team formation：项目技能数 `g = 4, 6, 8, 10`，每档 50 random projects。
  - Temporal keyword search：`|Gamma_q| = 2..7`，默认 4；每次 100 queries。
- `f`：
  - ECML PKDD team formation未直接给每个技能的候选数；由 actor/movie genre 或 title keyword 倒排得到。
  - Temporal keyword search 将 `f` 定义为 query label frequency，取 `100, 200, 300, 400, 500`，默认 300。
- 适配说明：IMDB 可以形成多个难度层次，从几千点 actor-skill 图到百万级关系图。若目标是 `g <= 22` 的精确 GST，建议先用 ECML PKDD team graph 或 STAR 子图。

## 4. YAGO Knowledge Graph

- 论文：STAR, "STAR: Steiner-Tree Approximation in Relationship Graphs." ICDE 2009.
- PDF：https://resources.mpi-inf.mpg.de/yago-naga/naga/download/ICDEResearchLong09_264.pdf
- 原始场景：YAGO 知识库，由 Wikipedia 半结构化信息抽取，并与 WordNet taxonomy 集成。
- 图类型：知识图谱/实体关系图；节点是实体和类，边是 facts、`type`、`subClassOf` 等关系；论文把关系视为可无向遍历。
- 图规模：1.7M nodes、14M edges。
- 权重：每条边有 YAGO confidence score，论文将 confidence 转为 distance measure。
- group 来源：STAR 原实验是给定实体 terminals；若转普通 GST，就是 singleton groups。若转 keyword GST，可通过实体标签和类别标签构造 keyword -> matching entities 的 groups。
- `g`：论文生成 2 组 queries，分别有 3 和 6 terminals；每组 30 random queries。还测试 top-1、top-3、top-6 answers。
- `f`：terminal 查询下 `f = 1`。若改为 keyword search，`f` 由 keyword 命中的实体数决定，原 STAR 实验未报告。
- 适配说明：可作为大规模知识图谱压力测试；若直接全图运行精确 GST 很重，建议先按 query 周围半径抽取局部子图。

## 5. Freebase + Free917

- 论文：Shuo Han, Lei Zou, Jeffery Xu Yu, Dongyan Zhao. "Keyword Search on RDF Graphs: A Query Graph Assembly Approach." CIKM 2017.
- arXiv PDF：https://arxiv.org/pdf/1704.00205
- 原始场景：Freebase RDF 知识库上的问答/关键词查询。
- 图类型：RDF graph / knowledge graph；subject/object 为顶点，predicate 为有向边。适配当前框架时可无向化，也可把 predicate 节点化。
- 图规模：153M entities、19K relations、15K classes、1.9B triples。
- group 来源：Free917 问答被人工改写为 keyword queries；每个 keyword 的 entity/class/relation candidates 可作为 group。
- `g`：论文选取 Free917 中 80 个 questions 并人工重写为 keyword queries；未在表中给每个 query 的 keyword 数分布。
- `f`：候选上限参数 `k` 是每个 keyword term 允许匹配的最大候选 entity/class/predicate 数；论文复杂度中使用该 `k`，但未给实验中固定 `f` 表。用于 GST 时可设 top-`f` candidates。
- 适配说明：非常大，不适合直接进入当前精确算法；适合作为局部 KG 子图抽取来源。

## 6. Austin / Houston / Los Angeles GTFS + OSM POI

- 论文：JIS 2023, "An Efficient Dynamic Programming Algorithm for Finding Group Steiner Trees in Temporal Graphs."
- PDF：https://cse.hkust.edu.hk/~raywong/paper/JIS23-SteinerTreeTemporalGraph.pdf
- 原始场景：城市公共交通网络。GTFS 记录公交/交通时刻表；OpenStreetMap 提取站点 500 米内 POI。
- 图类型：temporal directed graph；节点是 station，边是一次从站点到站点的公交/交通移动，带出发/到达时间和旅行代价；节点带 POI type labels。
- 图规模：
  - Austin：2.7K vertices、535.5K temporal edges，平均度 119.6；最大连通分量 2.5K vertices、506.1K edges。
  - Houston：9.1K vertices、1,796.2K edges，平均度 197.4；最大连通分量 5.2K vertices、1,066.1K edges。
  - Los Angeles：14.0K vertices、1,986.8K edges，平均度 142.0；最大连通分量 6.0K vertices、1,143.5K edges。
- group 来源：query labels 是 POI types；每个 POI type 对应带该 label 的 station 集合。
- `g`：论文记为 `kn`，取 `4, 5, 6, 7, 8`，默认 4。
- `f`：论文记为 `lf`，即每个 query label 平均覆盖顶点数，取 `100, 200, 400, 600, 800`，默认 400。
- 查询数：每组参数生成 30 queries。
- 适配说明：若忽略时间，可转为静态无向加权图；边权可取最短旅行时间、距离或单位权。该数据集能补足交通/POI 场景。

## 7. SNAP Temporal / Wiki-Fr Temporal

- 论文：Ge, Chen, Liu. "An Efficient Keywords Search in Temporal Social Networks." Data Science and Engineering 2023.
- 论文页面：https://link.springer.com/doi/10.1007/s41019-023-00218-7
- 表 2：https://link.springer.com/article/10.1007/s41019-023-00218-7/tables/2
- 原始场景：
  - SNAP：Wikipedia users editing each other's Talk page，节点是用户，边是交互。
  - Wiki-Fr：French Wikipedia article / user temporal interaction graph。
- 图类型：temporal social graph；边带交互时间区间；节点带用户名或文本 labels。适配当前框架时可转静态无向图。
- 图规模：
  - SNAP：256K vertices、420K edges、平均度 1.6、20.0K time instants。
  - Wiki-Fr：2,210K vertices、4,412K edges、平均度 2.0、21.3K time instants。
- group 来源：query labels / keywords；每个 label 命中的用户或文章节点构成 group。
- `g`：论文记为 `|Gamma_q|`，取 `{2, 3, 4, 5, 6, 7}`，默认 4。
- `f`：论文记为 label frequency，取 `{100, 200, 300, 400, 500}`，默认 300。
- 查询数：每组参数生成 100 queries。
- 适配说明：大图低平均度，适合测试连通性过滤、状态剪枝和大规模稀疏图表现。

## 8. US Patent 关系数据库图

- 来源：BANKS / EMBANKS keyword search over relational databases。
- 相关全文：https://arxiv.org/pdf/1104.4384
- 原始场景：美国专利数据库，包含 patent、inventor、category、citation、company/name 等表。
- 图类型：关系数据库 tuple graph；节点是 tuple，边来自 foreign-key / citation 等关系；通常是 directed graph，适配时可双向化。
- 图规模：约 2.408M nodes、5.294M directed edges。
- group 来源：keyword search。每个 keyword 通过 symbol table / inverted index 映射到一批 tuple nodes。
- `g`：原文未给固定 query size；BANKS 类查询通常为用户输入多个 keywords。
- `f`：由每个 keyword 命中的 tuple 数决定，原文未列全局分档。
- 适配说明：可补充专利/引用网络场景，但需要构造公开可复现的查询集。

## 9. IIT Bombay ETD

- 来源：BANKS / EMBANKS keyword search。
- 相关全文：https://arxiv.org/pdf/1104.4384
- 原始场景：高校电子学位论文数据库。
- 图类型：关系数据库 tuple graph；实体包括 department、faculty、program、student、thesis 等。
- 图规模：4,329 nodes、10,754 directed edges。
- group 来源：keyword 命中的 tuple nodes。
- `g`：原文未给固定 query size。
- `f`：由 keyword 命中数决定，原文未列。
- 适配说明：规模小，适合作为新数据格式和查询生成流程的 debug dataset。

## 10. Team Formation DBLP Expert-Skill Graph

- 论文：Kargar, An, Zihayat. "Efficient Bi-objective Team Formation in Social Networks." ECML PKDD 2012.
- PDF：https://www.eecs.yorku.ca/~aan/research/paper/pkdd12.pdf
- 原始场景：专家团队形成。项目要求若干 skills，需要选择专家覆盖全部 skills，同时最小化沟通成本/人员成本。
- 图类型：无向加权 expert collaboration graph；节点是专家，边是合作关系，边权是沟通成本。
- 图构造：
  - 使用 DBLP XML。
  - 仅取若干主要 CS 会议：SIGMOD、VLDB、ICDE、ICDT、EDBT、PODS、KDD、WWW、SDM、PKDD、ICDM、ICML、ECML、COLT、UAI、SODA、FOCS、STOC、STACS。
  - expert 是至少有 3 篇论文的作者。
  - skill 是该 expert 至少 2 篇论文标题中出现的 keyword/term。
  - 两位专家至少共同发表 2 篇论文则连边。
  - 边权为 `1 - |P_i cap P_j| / |P_i cup P_j|`。
- 图规模：5,658 nodes、8,588 edges。
- group 来源：每个 required skill 对应具备该 skill 的专家集合。
- `g`：项目技能数为 `4, 6, 8, 10`。
- `f`：原文未列每个 skill 的候选专家数；可从 expert-skill 倒排表直接统计。每档 `g` 生成 50 random projects。
- 适配说明：这是和 GST 最贴近的非仓库现有数据之一，图规模适中，组定义明确。

## 11. ACM / UMP / IMDB Skill Datasets

- 论文：Rehman et al. "A novel state space reduction algorithm for team formation in social networks." PLOS ONE 2021.
- 论文页面：https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0259786
- 数据来源：
  - UMP：https://github.com/MAK660/Dataset/blob/master/Staff_Expertise_DataSet.txt
  - DBLP：https://github.com/MAK660/Dataset/blob/master/DBLP_DataSet.txt
  - ACM：https://github.com/MAK660/Dataset/blob/master/ACM_DataSet.txt
  - IMDB：https://github.com/MAK660/Dataset/blob/master/IMDB_DataSet.txt
- 原始场景：团队形成 / expert-skill 覆盖。
- 图类型：原始公开文件主要是 expert -> skill list；需要额外构造 expert graph。常见做法是按共同技能、共同论文、共同电影或组织关系连边，边权用 Jaccard distance。
- 公开文件规模：
  - UMP staff expertise：约 90 条 staff-skill 记录。
  - DBLP skill file：5,642 行 expert-skill 记录。
  - ACM skill file：3,857 行 expert-skill 记录。
  - IMDB skill file：1,015 行 actor/worker-skill 记录。
- group 来源：每个 skill 是一个 group，候选是具备该 skill 的 experts。
- `g`：PLOS ONE 页面未给统一固定的 query skill 数表；相关团队形成实验常用 4-20 skills 或 4/6/8/10 skills。落地时建议与 ECML PKDD 设置对齐为 `g = 4, 6, 8, 10`。
- `f`：由 skill -> experts 倒排统计；公开文件可直接计算每个 skill 的 frequency。
- 适配说明：数据下载方便，但不是完整图，需要先定义边构造规则；适合作为 team-formation 场景扩展池。

## 12. RDF Keyword Search Benchmarks

- 论文：Automatic Construction of Benchmarks for RDF Keyword Search Systems Evaluation, ICEIS 2021.
- PDF：https://www.scitepress.org/Papers/2021/105194/105194.pdf
- 数据/benchmark：
  - RDF datasets：https://doi.org/10.6084/m9.figshare.11347676.v3
  - queries / solution generators / statistics：https://doi.org/10.6084/m9.figshare.9943655.v12
- 原始场景：RDF keyword search benchmark，自动生成 keyword queries 和 correct answers。
- 图类型：RDF graph；subject/object 为节点，predicate 为有向边。可转无向 GST 图；keyword 匹配资源作为 groups。
- 数据集：
  - Full Mondial RDF。
  - Full IMDb RDF。
  - DBpedia subset。
  - LUBM synthetic RDF。
  - BSBM synthetic RDF。
- `g`：
  - IMDb：35 keyword queries 来自 Coffman benchmark。
  - Mondial：24 keyword queries 来自 Coffman benchmark。
  - DBpedia：50 keyword queries 来自 Dosso benchmark。
  - LUBM：14 queries。
  - BSBM：13 queries。
  - 单条 query 的 `g` 等于 keyword 数，论文示例有 2-5 keywords，如 `niger country`、`poland cape verde organization`、`Captain America creator notable works`。
- `f`：
  - 算法参数 `sigma2 = 5`，即每轮最多保留 top-5 seed resources，可视作候选截断后的每组频率上限。
  - 示例 selected seeds：Mondial 多为 2-5；IMDb 示例均为 5；DBpedia 示例均为 5。
- 适配说明：这是构造 GST query 文件的好来源，尤其适合用现成 queries 和 solution generators 检查语义合理性。

## 13. VLSI Rectilinear Group Steiner Tree

- 论文：A. Rohe, M. Zachariasen. "Rectilinear group Steiner trees and applications in VLSI design." Mathematical Programming 2003.
- 论文页面：https://link.springer.com/article/10.1007/s10107-002-0326-x
- 原始场景：VLSI detailed routing。一个 logical unit 可能有多个 electrically equivalent ports，只需要在每个 group 中选择一个 pin/port 连接。
- 图类型：rectilinear group Steiner tree；通常先构造 Hanan grid，再在网格图上求解；边权是 Manhattan / L1 distance。
- 图规模：论文报告真实 VLSI instances，最多 100 groups；搜索摘要未给完整节点/边表，需要获取论文或实例包进一步确认。
- group 来源：每个 logical unit / net 的等价 pin 集合。
- `g`：最多 100 groups；可为当前框架抽取 `g <= 22` 的子实例。
- `f`：每组是等价 pin/port 数，原摘要未给分布；需从实例文件统计。
- 适配说明：这是最“原生”的 GST 场景之一，和 keyword search 无关；难点是公开实例获取、Hanan grid 生成与格式转换。

## 14. SteinLib VLSI-Derived Grid Graphs

- 数据库：https://steinlib.zib.de/steinlib.php
- testsets 页面：https://steinlib.zib.de/testset.php
- 原始场景：Steiner tree benchmark，包含大量工业/VLSI 派生网格图。
- 图类型：grid graphs with holes，L1 weights；多为普通 Steiner Tree Problem，也包含由 group Steiner 转换成普通 STP 的实例。
- 相关 testsets：ALUE、ALUT、DIW、DMXA、GAP、MSM、TAQ、LIN。
- 图规模：约 53 到 38,418 nodes；例如 ALUE 940-34,479 nodes，ALUT 387-36,711 nodes，DIW 212-11,821 nodes，LIN 53-38,418 nodes。
- group 来源：若使用原始 SteinLib STP 文件，多数只有 terminals，没有原始 groups；需要寻找对应 group 原始实例，或把普通 STP 当作 singleton groups。
- `g`：普通 STP 中 `g = #terminals` 且 `f = 1`；若能拿到 GSTP 原始实例，则 `g = #groups`。
- `f`：普通 STP 为 1；GSTP 原始 group size 需从实例统计。
- 适配说明：适合作为 graph topology / hard graph 来源；若目标明确是 GST group-size 实验，不应只使用转换后的 singleton terminal 版本。

## 推荐落地顺序

1. **Mondial**：小中规模、查询明确、图结构非平凡，最适合先接入。
2. **Team Formation DBLP Expert-Skill**：`g` 明确为 4/6/8/10，图规模适中，group 定义天然。
3. **Amazon PGST**：补足电商场景，规模大但原始数据和 keyword 信息清楚。
4. **Austin / Houston / LA GTFS + OSM**：`g` 和 `f` 原文完整，适合交通/POI 场景。
5. **IMDB actor-skill 或 RDF benchmark**：电影场景，可做多个规模层次。
6. **SNAP / Wiki-Fr temporal**：大规模稀疏社交图，适合压力测试。
7. **YAGO / Freebase**：知识图谱超大图，建议只做局部抽样。
8. **VLSI RGST / SteinLib**：理论上最贴近 GST，但需要额外确认实例获取与 group 信息。

## 需要后续补查或计算的字段

- 对多数 keyword/RDF/team datasets，`f` 最可靠的方式是从原始数据建立倒排表后统计，而不是只引用论文摘要。
- `Amazon PGST`、`ACM/UMP/IMDB skill files`、`VLSI RGST` 的实验 `g/f` 分布需要进一步读代码或实例文件。
- `YAGO`、`Freebase` 全图过大，建议先定义 query-centered subgraph extraction，再统计有效 `n/m/g/f`。
