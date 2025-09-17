// src/histogram.cpp
// Compile: g++ -O3 -std=c++17 src/histogram.cpp -o histogram -pthread
// For OpenMP-enabled build: g++ -O3 -std=c++17 src/histogram.cpp -o histogram -fopenmp

#include <bits/stdc++.h>
#ifdef _OPENMP
  #include <omp.h>
#endif

using ull = unsigned long long;
using u64 = uint64_t;
using i64 = int64_t;

// --------------------------- HELPERS ------------------------------------

/**
 * get_time_sec
 * Return time in seconds as double using steady_clock.
 * This is used for std::thread timing. For OpenMP-specific code, omp_get_wtime() is used.
 */
static double get_time_sec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/**
 * parse_args
 * Very simple parser for command line arguments. Supports:
 * --size N --range R --seed S --threads T --backend {std|openmp}
 * --method {private|mutex|atomic} --reps R --quiet
 */
struct Config {
    size_t N = 100000000;     // default 100M
    int range = 256;
    uint64_t seed = 123456789ULL;
    int threads = 0;          // 0 => auto detect
    std::string backend = "std"; // "std" or "openmp"
    std::string method = "private"; // private, mutex, atomic
    int reps = 3;
    bool quiet = false;
};
Config parse_args(int argc, char** argv) {
    Config cfg;
    for(int i=1;i<argc;i++){
        std::string s = argv[i];
        if(s=="--size" && i+1<argc) cfg.N = std::stoull(argv[++i]);
        else if(s=="--range" && i+1<argc) cfg.range = std::stoi(argv[++i]);
        else if(s=="--seed" && i+1<argc) cfg.seed = std::stoull(argv[++i]);
        else if(s=="--threads" && i+1<argc) cfg.threads = std::stoi(argv[++i]);
        else if(s=="--backend" && i+1<argc) cfg.backend = argv[++i];
        else if(s=="--method" && i+1<argc) cfg.method = argv[++i];
        else if(s=="--reps" && i+1<argc) cfg.reps = std::stoi(argv[++i]);
        else if(s=="--quiet") cfg.quiet = true;
        else {
            std::cerr << "Unknown arg " << s << "\n";
            std::exit(1);
        }
    }
    if(cfg.threads <= 0) {
#ifdef _OPENMP
        if(cfg.backend=="openmp") cfg.threads = omp_get_max_threads();
        else
#endif
            cfg.threads = std::thread::hardware_concurrency()>0 ? std::thread::hardware_concurrency() : 1;
    }
    return cfg;
}

// --------------------------- RANDOM GENERATION --------------------------

/**
 * Thread-local RNG factory: returns an mt19937 seeded deterministically from global seed and thread id.
 * Using thread_local ensures each thread has its own engine.
 */
static inline std::mt19937 make_thread_rng(uint64_t global_seed, uint32_t tid) {
    // Create a seed sequence from global_seed and tid for reproducibility
    std::seed_seq seq{ (uint32_t)(global_seed & 0xffffffffu),
                       (uint32_t)((global_seed>>32) & 0xffffffffu),
                       tid };
    std::mt19937 eng(seq);
    return eng;
}

/**
 * fill_data_stdthreads
 * Fill `data` in parallel using std::thread. Each thread has a thread_local RNG seeded with seed+tid.
 */
void fill_data_stdthreads(std::vector<int>& data, uint64_t seed, int range, int threads) {
    size_t N = data.size();
    std::vector<std::thread> thr;
    thr.reserve(threads);
    auto worker = [&](int tid){
        // Create local RNG for this thread
        std::mt19937 rng = make_thread_rng(seed, (uint32_t)tid);
        std::uniform_int_distribution<int> dist(0, range-1);
        size_t chunk = (N + threads - 1) / threads;
        size_t start = tid * chunk;
        size_t end = std::min(N, start + chunk);
        for(size_t i=start;i<end;i++) data[i] = dist(rng);
    };
    for(int t=0;t<threads;t++) thr.emplace_back(worker, t);
    for(auto &t: thr) t.join();
}

#ifdef _OPENMP
/**
 * fill_data_openmp
 * Fill `data` in parallel using OpenMP parallel for. Each thread seeds an mt19937
 * with omp_get_thread_num() combined with provided seed.
 */
void fill_data_openmp(std::vector<int>& data, uint64_t seed, int range, int threads) {
    size_t N = data.size();
    omp_set_num_threads(threads);
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::mt19937 rng = make_thread_rng(seed, (uint32_t)tid);
        std::uniform_int_distribution<int> dist(0, range-1);
        #pragma omp for schedule(static)
        for(size_t i=0;i<N;i++){
            data[i] = dist(rng);
        }
    }
}
#endif

// --------------------------- HISTOGRAM VARIANTS -------------------------

/**
 * compute_hist_private_threads
 * Each thread builds a private histogram (vector<u64>), then reduce by summing
 * all private histograms into a global result.
 *
 * Returns elapsed time in seconds.
 */
double compute_hist_private_threads(const std::vector<int>& data, int range, std::vector<u64>& out_hist, int threads) {
    size_t N = data.size();
    double t0 = get_time_sec();
    // allocate per-thread histograms
    std::vector<std::vector<u64>> local(threads, std::vector<u64>(range, 0));
    std::vector<std::thread> thr;
    thr.reserve(threads);
    auto worker = [&](int tid){
        size_t chunk = (N + threads - 1) / threads;
        size_t start = tid * chunk;
        size_t end = std::min(N, start + chunk);
        auto &lh = local[tid];
        for(size_t i=start;i<end;i++){
            ++lh[(size_t)data[i]];
        }
    };
    for(int t=0;t<threads;t++) thr.emplace_back(worker, t);
    for(auto &t: thr) t.join();
    // reduce
    for(int t=0;t<threads;t++){
        for(int v=0; v<range; v++) out_hist[v] += local[t][v];
    }
    double t1 = get_time_sec();
    return t1 - t0;
}

/**
 * compute_hist_mutex
 * Global histogram protected by a mutex - simple but high contention.
 */
double compute_hist_mutex(const std::vector<int>& data, int range, std::vector<u64>& out_hist, int threads) {
    size_t N = data.size();
    std::mutex mtx;
    double t0 = get_time_sec();
    std::vector<std::thread> thr;
    auto worker = [&](int tid){
        size_t chunk = (N + threads - 1) / threads;
        size_t start = tid * chunk;
        size_t end = std::min(N, start + chunk);
        for(size_t i=start;i<end;i++){
            int v = data[i];
            std::lock_guard<std::mutex> lock(mtx);
            ++out_hist[v];
        }
    };
    for(int t=0;t<threads;t++) thr.emplace_back(worker, t);
    for(auto &t: thr) t.join();
    double t1 = get_time_sec();
    return t1 - t0;
}

/**
 * compute_hist_atomic
 * Global histogram using std::atomic<uint64_t> for increments.
 */
double compute_hist_atomic(const std::vector<int>& data, int range, std::vector<u64>& out_hist, int threads) {
    size_t N = data.size();
    // Use atomic vector for safe concurrent increments
    std::vector<std::atomic<u64>> ah(range);
    for(int i=0;i<range;i++) ah[i].store(0);
    double t0 = get_time_sec();
    std::vector<std::thread> thr;
    auto worker = [&](int tid){
        size_t chunk = (N + threads - 1) / threads;
        size_t start = tid * chunk;
        size_t end = std::min(N, start + chunk);
        for(size_t i=start;i<end;i++){
            ++ah[(size_t)data[i]];
        }
    };
    for(int t=0;t<threads;t++) thr.emplace_back(worker, t);
    for(auto &t: thr) t.join();
    // copy back
    for(int v=0; v<range; v++) out_hist[v] = ah[v].load();
    double t1 = get_time_sec();
    return t1 - t0;
}

#ifdef _OPENMP
/**
 * compute_hist_private_openmp
 * OpenMP version of private hist + reduction.
 * Uses threadprivate local hist or local vectors and then reduces.
 */
double compute_hist_private_openmp(const std::vector<int>& data, int range, std::vector<u64>& out_hist, int threads) {
    size_t N = data.size();
    omp_set_num_threads(threads);
    double t0 = omp_get_wtime();
    // create per-thread local hist (flattened) to avoid many allocations
    int T = threads;
    std::vector<u64> flat((size_t)T * range, 0);
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        size_t start = (size_t)tid * (N + T - 1) / T;
        size_t end = std::min(N, start + (N + T - 1) / T);
        u64* mybase = &flat[(size_t)tid * range];
        #pragma omp for schedule(static) nowait
        for(size_t i=start;i<end;i++){
            ++mybase[data[i]];
        }
    }
    // reduce
    for(int t=0;t<T;t++){
        u64* base = &flat[(size_t)t*range];
        for(int v=0; v<range; v++) out_hist[v] += base[v];
    }
    double t1 = omp_get_wtime();
    return t1 - t0;
}

/**
 * compute_hist_atomic_openmp
 * OpenMP version using atomic pragma for increment.
 */
double compute_hist_atomic_openmp(const std::vector<int>& data, int range, std::vector<u64>& out_hist, int threads) {
    size_t N = data.size();
    omp_set_num_threads(threads);
    double t0 = omp_get_wtime();
    #pragma omp parallel for schedule(static)
    for(size_t i=0;i<N;i++){
        int v = data[i];
        #pragma omp atomic
        out_hist[v]++;
    }
    double t1 = omp_get_wtime();
    return t1 - t0;
}
#endif

// --------------------------- MAIN / RUNNER ------------------------------

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    // Print config to stderr (or not if quiet)
    if(!cfg.quiet) {
        std::cerr << "Config: N=" << cfg.N << " range=" << cfg.range << " seed=" << cfg.seed
                  << " threads=" << cfg.threads << " backend=" << cfg.backend
                  << " method=" << cfg.method << "\n";
    }

    // allocate data
    std::vector<int> data(cfg.N);

    // Fill data (parallel)
    if(cfg.backend == "openmp") {
#ifdef _OPENMP
        if(!cfg.quiet) std::cerr << "Filling data with OpenMP using " << cfg.threads << " threads...\n";
        fill_data_openmp(data, cfg.seed, cfg.range, cfg.threads);
#else
        std::cerr << "OpenMP backend requested but binary not compiled with OpenMP support.\n";
        return 1;
#endif
    } else {
        if(!cfg.quiet) std::cerr << "Filling data with std::thread using " << cfg.threads << " threads...\n";
        fill_data_stdthreads(data, cfg.seed, cfg.range, cfg.threads);
    }

    // Run the experiment reps times and print results CSV style:
    // variant,backend,threads,rep,time_sec,total_count,valid_sum
    for(int rep=0; rep<cfg.reps; rep++){
        std::vector<u64> hist(cfg.range, 0);
        double elapsed = 0.0;
        if(cfg.backend == "openmp"){
#ifdef _OPENMP
            if(cfg.method == "private") elapsed = compute_hist_private_openmp(data, cfg.range, hist, cfg.threads);
            else if(cfg.method == "atomic") elapsed = compute_hist_atomic_openmp(data, cfg.range, hist, cfg.threads);
            else if(cfg.method == "mutex") {
                // fallback to mutex-based std::thread implementation (mutex variant is generic)
                elapsed = compute_hist_mutex(data, cfg.range, hist, cfg.threads);
            } else {
                std::cerr << "Unknown method\n"; return 1;
            }
#else
            std::cerr << "OpenMP backend but not compiled with -fopenmp\n"; return 1;
#endif
        } else { // std backend
            if(cfg.method == "private") elapsed = compute_hist_private_threads(data, cfg.range, hist, cfg.threads);
            else if(cfg.method == "atomic") elapsed = compute_hist_atomic(data, cfg.range, hist, cfg.threads);
            else if(cfg.method == "mutex") elapsed = compute_hist_mutex(data, cfg.range, hist, cfg.threads);
            else { std::cerr << "Unknown method\n"; return 1; }
        }

        // verify sum
        u64 sum = 0;
        for(auto &v: hist) sum += v;
        bool ok = (sum == cfg.N);
        std::cout << cfg.method << "," << cfg.backend << "," << cfg.threads << "," << rep << "," << elapsed << "," << sum << "," << (ok? "OK":"BAD") << "\n";
    }

    return 0;
}
