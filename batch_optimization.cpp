/**
 * Study batch optimization, that maximizes RAM bandwidth usage, by issuing a lot of load commands all
 * at once (for a batch), then letting the processor run a "payload" of other instructions. Essentially,
 * since all the load from main memory commands are done in parallel, it's faster than issuing a load,
 * processing an instruction, issuing another load, processing another instruction...
 *
 * Observations:
 * - prefetch works differently on Mac and Linux
 * - benefits of batching/pre-fetching reduced as payload increases
 * - optimal size of batch has wide plateau
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <numeric>
#include <iterator>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <map>
#include <fstream>

using namespace std;
using namespace chrono;

/**
 * This class just dumps all the numbers to a clearly labeled file for post-processing.
 */
class Stats
{
public:
  Stats(const string function_name, size_t M, size_t N, size_t n_reps, size_t data_bytes,
        bool aligned, const string time_unit, const string hash_function_name,
        const string notes = "")
    : _function_name(function_name)
  {
    stringstream filename;
    filename << function_name << "."
    << hash_function_name << "."
    << M << "." << N << "." << n_reps << "." << data_bytes
    << (aligned ? ".aligned." : ".unaligned.")
    << time_unit << "-" << notes << ".txt";
    _out = new ofstream(filename.str());
  }

  ~Stats() {
    _out->close();
    delete _out;
    _out = nullptr;
  }

  void running_stats(size_t batch_size, const string algo, const vector<double> &times)
  {
    cout << _function_name << " " << algo << " " << batch_size << " ";
    copy(times.begin(), times.end(), ostream_iterator<double>(cout, " "));
    cout << endl;
    *_out << algo << " " << batch_size << " ";
    copy(times.begin(), times.end(), ostream_iterator<double>(*_out, " "));
    *_out << endl;
  }

private:
  const string _function_name;
  ofstream* _out;
};

/**
 * Hash function used to locate data to process (18 instructions in gcc 4.8.2)
 *
        movq    %rdi, %rdx
        movabsq $2388976653695081527, %rcx
        movabsq $4619197404915747639, %rax
        shrq    $23, %rdx
        movabsq $-8645972361240307355, %rsi
        xorq    %rdi, %rdx
        imulq   %rcx, %rdx
        xorq    %rdx, %rax
        shrq    $47, %rdx
        xorq    %rdx, %rax
        imulq   %rsi, %rax
        movq    %rax, %rdi
        shrq    $23, %rdi
        xorq    %rax, %rdi
        imulq   %rcx, %rdi
        movq    %rdi, %rax
        shrq    $47, %rax
        xorq    %rdi, %rax
        ret
 */
inline size_t hash64_2(size_t n, size_t seed1, size_t seed2) {
  const size_t m = 0x880355f21e6d1965ULL;
  const size_t* pos = &n;
  const size_t len = sizeof(size_t);
  const size_t* end = pos + (len / 8);
  const unsigned char* pos2;
  size_t h = (seed1 + seed2) ^ (len * m);
  size_t v;

  auto mix = [](size_t hh) {
    (hh) ^= (hh) >> 23;
    (hh) *= 0x2127599bf4325c37ULL;
    (hh) ^= (hh) >> 47;
    return hh;
  };

  while (pos != end) {
    v = *pos++;
    h ^= mix(v);
    h *= m;
  }

  pos2 = (const unsigned char*)pos;
  v = 0;

  switch (len & 7) {
    case 7:
      v ^= (size_t)pos2[6] << 48;
    case 6:
      v ^= (size_t)pos2[5] << 40;
    case 5:
      v ^= (size_t)pos2[4] << 32;
    case 4:
      v ^= (size_t)pos2[3] << 24;
    case 3:
      v ^= (size_t)pos2[2] << 16;
    case 2:
      v ^= (size_t)pos2[1] << 8;
    case 1:
      v ^= (size_t)pos2[0];
      h ^= mix(v);
      h *= m;
    default: ;
  }

  return mix(h);
}

// The various functions we want to use as "payload"
typedef int (*PayloadFunction)(int);

inline int id(int x) { return x; }

/**
 * p1 is about 17 instructions in gcc 4.8.2
        movzbl  %dil, %eax
        pushq   %rbx
        movl    %edi, %ebx
        xorl    $-2128831035, %eax
        movzbl  %bh, %esi
        movl    %edi, %ecx
        imull   $16777619, %eax, %edx
        shrl    $16, %ecx
        shrl    $24, %edi
        movzbl  %cl, %r9d
        popq    %rbx
        xorl    %esi, %edx
        imull   $16777619, %edx, %r8d
        xorl    %r9d, %r8d
        imull   $16777619, %r8d, %r10d
        xorl    %edi, %r10d
        imull   $16777619, %r10d, %eax
        ret
 */
inline int p1(int x) {
  const unsigned char *ptr = (const unsigned char *) &x;
  const uint32_t Prime = 0x01000193; //   16777619
  uint32_t hash  = 0x811C9DC5; // 2166136261
  hash = (*ptr++ ^ hash) * Prime;
  hash = (*ptr++ ^ hash) * Prime;
  hash = (*ptr++ ^ hash) * Prime;
  return (*ptr ^ hash) * Prime;
}

template <size_t n>
inline int pn(int x) { for (size_t i = 0; i < n; ++i) x = p1(x); return x; }
inline int trig(int x) { return (int) ((int) ((int) cos(x) + sin(x)) / (1+log(x)));  }

// The main testing function
long test(size_t M, size_t N, size_t n_reps, PayloadFunction F, int* data, Stats& stats) {

  // The list of batch_sizes we are interested in
  vector<size_t> batch_sizes;
  for (size_t i = 2; i <= 80; i += 2)
    batch_sizes.push_back(i);

  vector<double> times(n_reps); // record times
  // a "certificate" to prevent the compiler from optimizing away the loops
  long certificate1 = 0;

  { // random accesses - no batching
    for (size_t k = 0; k < n_reps; ++k) {
      auto t1 = high_resolution_clock::now();
      for (size_t i = 0; i < N; ++i) {
        size_t pos = hash64_2(i, k, N) % M;
        certificate1 += F(data[pos]);
      }
      auto t2 = high_resolution_clock::now();
      times[k] = duration_cast<microseconds>(t2 - t1).count();
    }
    stats.running_stats(0, "no batch", times);
  }

  { // batching: we group all the reads in a batch and let the hardware prefetch
    for (size_t batch_size : batch_sizes) {
      int batch[batch_size];
      long certificate2 = 0;
      for (size_t k = 0; k < n_reps; ++k) {
        auto t1 = high_resolution_clock::now();
        size_t last = (N / batch_size) * batch_size;
        for (size_t i = 0; i < last; i += batch_size) {
          for (size_t j = 0; j < batch_size; ++j) {
            size_t pos = hash64_2(i + j, k, N) % M;
            batch[j] = data[pos];
          }
          for (size_t j = 0; j < batch_size; ++j) {
            certificate2 += F(batch[j]);
          }
        }
        for (size_t i = last; i < N; ++i) {
          size_t pos = hash64_2(i, k, N) % M;
          certificate2 += F(data[pos]);
        }
        auto t2 = high_resolution_clock::now();
        times[k] = duration_cast<microseconds>(t2 - t1).count();
      }
      if (certificate2 != certificate1) {
        cerr << "Error in batching algorithm - certificates don't match" << endl;
        exit(1);
      }
      stats.running_stats(batch_size, "batch only", times);
    }
  }

  { // batching/pre-fetching: when we process values, we manually pre-fetch the _next_ batch
    // note: pre-fetching more than 1 batch is slower
    // Note: on Mac, batch/prefetch doesn't always work (depending on payload). Works better on Linux.
    for (size_t batch_size : batch_sizes) {
      int batch[batch_size];
      long certificate3 = 0;
      size_t hashes[batch_size]; // to avoid recomputing hashes twice, small so no impact on cache (?)
      for (size_t k = 0; k < n_reps; ++k) {
        auto t1 = high_resolution_clock::now();
        size_t last = (N / batch_size) * batch_size;
        for (size_t i = 0; i < last; i += batch_size) {
          for (size_t j = 0; j < batch_size; ++j) {
            size_t pos = i > 0 ? hashes[j] : hash64_2(i + j, k, N) % M;
            batch[j] = data[pos];
          }
          for (size_t j = batch_size; j < 2*batch_size; ++j) {
            size_t pos = hashes[j-batch_size] = hash64_2(i + j, k, N) % M;
            __builtin_prefetch(data + pos, 0, 1);
          }
          for (size_t j = 0; j < batch_size; ++j) {
            certificate3 += F(batch[j]);
          }
        }
        for (size_t i = last; i < N; ++i) {
          size_t pos = hash64_2(i, k, N) % M;
          certificate3 += F(data[pos]);
        }
        auto t2 = high_resolution_clock::now();
        times[k] = duration_cast<microseconds>(t2 - t1).count();
      }
      if (certificate3 != certificate1) {
        cerr << "Error in batching with prefetching algorithm - certificates don't match" << endl;
        exit(1);
      }
      stats.running_stats(batch_size, "batch prefetch", times);
    }
  }

  { // sort/batching/pre-fetching: draw all, sort, then batch and prefetch
    // NOTE: one big sort upfront on the N locations is too slow.
    // NOTE: without the sort though, this seems to be pretty good for some reason,
    //       although the locations array takes some cache too
    for (size_t batch_size : batch_sizes) {
      int batch[batch_size];
      size_t locations[N];
      long certificate4 = 0;
      for (size_t k = 0; k < n_reps; ++k) {
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; ++i) {
          locations[i] = hash64_2(i, k, N) % M;
        }
        //sort(locations, locations + N); // way too slow
        size_t last = (N / batch_size) * batch_size;
        for (size_t i = 0; i < last; i += batch_size) {
          for (size_t j = 0; j < batch_size; ++j) {
            batch[j] = data[locations[i+j]]; // hmmm... locations is going to use up some cache!
            if (i+j+batch_size < N) {
              //__builtin_prefetch(locations + i+j+batch_size); // helps if very small batch size
              __builtin_prefetch(data + locations[i+j+batch_size]);
            }
          }
          for (size_t j = 0; j < batch_size; ++j) {
            certificate4 += F(batch[j]);
          }
        }
        for (size_t i = last; i < N; ++i) {
          size_t pos = hash64_2(i, k, N) % M;
          certificate4 += F(data[pos]);
        }
        auto t2 = high_resolution_clock::now();
        times[k] = duration_cast<microseconds>(t2 - t1).count();
      }
      if (certificate4 != certificate1) {
        cerr << "Error in batching with locations/batching algorithm - certificates don't match" << endl;
        exit(1);
      }
      stats.running_stats(batch_size, "locations batch", times);
    }
  }

  return certificate1;
}

int main(int, char**) {

  size_t M = 1024*1024*1024u; // data size in units of elements (sizeof(int) = 4 bytes)
  size_t N = 1048576; // number of iterations of the "algorithm"
  size_t n_reps = 100; // number of repetitions for statistics

  // Generate M bytes of random data that we'll be accessing later
  cout << "Generating data: " << (float(M)/(1024*1024*1024)) << " GB" << endl;

  int *data = (int *) valloc(M * sizeof(int));
  for (size_t i = 0; i < M; ++i) // has the effect of touching the mem, loading in cache
    data[i] = rand(); // values don't matter, except for detecting bugs via the certificates

  cout << "Measuring" << endl;

  int certificate = 0;

  vector<pair<string, PayloadFunction>> functions =
    {
      { "identity", id },
      { "math" , trig },
      { "p1", p1 },
      { "p2", pn<2> },
      { "p4", pn<4> },
      { "p6", pn<6> },
      { "p8", pn<8> },
      { "p10", pn<10> },
      { "p12", pn<12> },
      { "p14", pn<14> },
      { "p16", pn<16> },
      { "p18", pn<18> },
      { "p20", pn<20> },
      { "p22", pn<22> },
      { "p24", pn<24> },
      { "p26", pn<26> },
      { "p28", pn<28> },
      { "p30", pn<30> },
      { "p32", pn<32> },
      { "p64", pn<64> },
      { "p128", pn<128> }
    };

  for (auto f : functions) {
    Stats stats(f.first, M, N, n_reps, sizeof(int), true,
                "microseconds", "fast-hash-64", "xeon.5.2620.v2.linux.6.6.gcc.4.8.3.4.DNDEBUG.O3.unroll");
    certificate += test(M, N, n_reps, f.second, data, stats);
  }

  // Output the certificate so that the compiler is not tempted
  // to optimize away the loops
  return certificate;
}

