#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

#include "Core/Jobs/JobSystem.hpp"

using Faye::Jobs::JobHandle;
using Faye::Jobs::JobSystem;

TEST_CASE("smoke: 1000 independent jobs all run")
{
    JobSystem jobs(4);
    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;
    handles.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        handles.push_back(jobs.schedule([&counter] { counter.fetch_add(1); }));
    jobs.waitAll(handles);
    CHECK(counter.load() == 1000);
}

TEST_CASE("dependencies: a chain runs in order, every time")
{
    JobSystem jobs(4);
    for (int run = 0; run < 1000; ++run)   // repeat to shake out races
    {
        std::vector<int> order;            // safe: the chain serializes access
        JobHandle a = jobs.schedule([&order] { order.push_back(1); });
        JobHandle b = jobs.schedule([&order] { order.push_back(2); }, {{a}});
        JobHandle c = jobs.schedule([&order] { order.push_back(3); }, {{b}});
        jobs.wait(c);
        REQUIRE(order == std::vector<int>{1, 2, 3});
    }
}

TEST_CASE("diamond: the join sees both branches")
{
    JobSystem jobs(4);
    std::atomic<int> branches{0};
    int observed = -1;
    JobHandle a = jobs.schedule([] {});
    JobHandle b = jobs.schedule([&branches] { branches.fetch_add(1); }, {{a}});
    JobHandle c = jobs.schedule([&branches] { branches.fetch_add(1); }, {{a}});
    const JobHandle bc[] = {b, c};
    JobHandle d = jobs.schedule([&] { observed = branches.load(); }, bc);
    jobs.wait(d);
    CHECK(observed == 2);
}

TEST_CASE("depending on an already-finished job still runs")
{
    JobSystem jobs(2);
    JobHandle a = jobs.schedule([] {});
    jobs.wait(a);                          // a is complete (and likely recycled)
    bool ran = false;
    JobHandle b = jobs.schedule([&ran] { ran = true; }, {{a}});
    jobs.wait(b);
    CHECK(ran);                            // exercises the tryAddDependent fast path
}

TEST_CASE("nested wait inside a job does not deadlock")
{
    JobSystem jobs(2);                     // deliberately few workers
    std::atomic<int> inner{0};
    JobHandle outer = jobs.schedule([&] {
        std::vector<JobHandle> subs;
        for (int i = 0; i < 8; ++i)
            subs.push_back(jobs.schedule([&inner] { inner.fetch_add(1); }));
        jobs.waitAll(subs);                // must drain/steal, not block
    });
    jobs.wait(outer);
    CHECK(inner.load() == 8);
}

TEST_CASE("parallelFor covers every index exactly once")
{
    JobSystem jobs(4);
    for (size_t count : {size_t(0), size_t(1), size_t(7), size_t(100), size_t(1000)})
        for (size_t grain : {size_t(1), size_t(3), size_t(64), size_t(2000)})
        {
            CAPTURE(count);
            CAPTURE(grain);
            std::vector<std::atomic<int>> hits(count);
            JobHandle h = jobs.parallelFor(count, grain, [&hits](size_t begin, size_t end) {
                for (size_t i = begin; i < end; ++i)
                    hits[i].fetch_add(1);
            });
            jobs.wait(h);
            for (size_t i = 0; i < count; ++i)
                REQUIRE(hits[i].load() == 1);
        }
}

TEST_CASE("stale handles read as complete after slot recycling")
{
    JobSystem jobs(2);
    JobHandle stale = jobs.schedule([] {});
    jobs.wait(stale);
    for (int i = 0; i < 5000; ++i)         // churn: force slot reuse many times
        jobs.wait(jobs.schedule([] {}));
    CHECK(jobs.isComplete(stale));
    jobs.wait(stale);                      // must return immediately, not hang
}

TEST_CASE("main-thread jobs run only on the main thread")
{
    // Note the invariant is one-directional: mainThreadOnly jobs must run on
    // the main thread, but ordinary jobs may run ANYWHERE — including on the
    // main thread, which executes pending jobs itself inside wait().
    JobSystem jobs(2);
    std::thread::id mainSeen{};
    JobHandle m = jobs.scheduleMainThread([&] { mainSeen = std::this_thread::get_id(); });
    jobs.pumpMainThread();
    jobs.wait(m);
    CHECK(mainSeen == std::this_thread::get_id());
}

// Asserts a real worker (not the main thread) ran the job, so it is meaningless
// under FAYE_JOBS_SINGLE_THREADED, which forces every job inline on the caller.
#ifndef JOBS_SINGLE_THREADED
TEST_CASE("workers pick up jobs when the main thread never participates")
{
    JobSystem jobs(2);
    std::atomic<bool> ran{false};
    std::thread::id workerSeen{};
    jobs.schedule([&] {
        workerSeen = std::this_thread::get_id();
        ran.store(true, std::memory_order_release);
    });
    // Deliberately NOT jobs.wait(): the main thread must not execute anything,
    // so whoever ran the job has to be a worker.
    while (!ran.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(workerSeen != std::this_thread::get_id());
}
#endif

TEST_CASE("a main-thread job can depend on worker jobs")
{
    JobSystem jobs(2);
    std::atomic<int> stage{0};
    JobHandle w = jobs.schedule([&stage] { stage.store(1); });
    JobHandle m = jobs.scheduleMainThread([&stage] {
        CHECK(stage.load() == 1);
        stage.store(2);
    }, {{w}});
    jobs.wait(m);                          // wait() on the main thread pumps
    CHECK(stage.load() == 2);
}

TEST_CASE("single-threaded mode runs everything inline")
{
    JobSystem jobs(0);
    CHECK(jobs.isSingleThreaded());
    int order = 0;
    JobHandle a = jobs.schedule([&] { CHECK(order == 0); ++order; });
    jobs.schedule([&] { CHECK(order == 1); ++order; }, {{a}});
    CHECK(order == 2);                     // both already ran, no wait needed

    int covered = 0;
    JobHandle p = jobs.parallelFor(10, 3, [&covered](size_t begin, size_t end) {
        covered += int(end - begin);
    });
    jobs.wait(p);
    CHECK(covered == 10);
}

TEST_CASE("stress: random DAG of jobs completes with correct dependency ordering")
{
    JobSystem jobs(4);
    constexpr int kJobs = 2000;
    std::vector<std::atomic<bool>> done(kJobs);
    std::vector<JobHandle> handles;
    handles.reserve(kJobs);

    for (int i = 0; i < kJobs; ++i)
    {
        std::vector<JobHandle> deps;
        for (int back = 1; back <= 3; ++back)          // depend on up to 3 predecessors
            if (i - back * 7 >= 0)
                deps.push_back(handles[size_t(i - back * 7)]);

        const std::vector<int> depIndices = [&] {
            std::vector<int> v;
            for (int back = 1; back <= 3; ++back)
                if (i - back * 7 >= 0)
                    v.push_back(i - back * 7);
            return v;
        }();

        handles.push_back(jobs.schedule([&done, i, depIndices] {
            for (const int dep : depIndices)
                CHECK(done[size_t(dep)].load(std::memory_order_acquire));   // deps ran first
            done[size_t(i)].store(true, std::memory_order_release);
        }, deps));
    }

    jobs.waitAll(handles);
    for (int i = 0; i < kJobs; ++i)
        CHECK(done[size_t(i)].load());
}

// --- Failure handling (F3) -------------------------------------------------
// Before this, an exception escaping a job body reached workerMain and was
// std::terminate — and because finishJob was skipped, every waiter hung.

TEST_CASE("a throwing job does not terminate, and its waiter still returns")
{
    JobSystem jobs(4);
    const uint64_t before = jobs.failureCount();

    JobHandle handle = jobs.schedule([] { throw std::runtime_error("boom"); }, {}, "thrower");
    jobs.wait(handle);   // must not hang

    CHECK(jobs.failureCount() == before + 1);
}

TEST_CASE("a non-std exception is caught too")
{
    JobSystem jobs(4);
    const uint64_t before = jobs.failureCount();

    JobHandle handle = jobs.schedule([] { throw 42; }, {}, "int thrower");
    jobs.wait(handle);

    CHECK(jobs.failureCount() == before + 1);
}

TEST_CASE("failure poisons dependents: they are skipped, not run on absent inputs")
{
    JobSystem jobs(4);
    std::atomic<bool> dependentRan{false};

    JobHandle producer = jobs.schedule([] { throw std::runtime_error("no output"); }, {}, "producer");
    JobHandle consumer = jobs.schedule([&dependentRan] { dependentRan.store(true); },
                                       {{producer}}, "consumer");
    jobs.wait(consumer);   // completes despite the failure upstream

    CHECK_FALSE(dependentRan.load());
}

TEST_CASE("poison propagates transitively down a chain")
{
    JobSystem jobs(4);
    std::atomic<int> ran{0};

    JobHandle a = jobs.schedule([] { throw std::runtime_error("boom"); }, {}, "a");
    JobHandle b = jobs.schedule([&ran] { ran.fetch_add(1); }, {{a}}, "b");
    JobHandle c = jobs.schedule([&ran] { ran.fetch_add(1); }, {{b}}, "c");
    jobs.wait(c);

    CHECK(ran.load() == 0);
}

TEST_CASE("a failure does not poison unrelated jobs")
{
    JobSystem jobs(4);
    std::atomic<bool> siblingRan{false};

    JobHandle failing = jobs.schedule([] { throw std::runtime_error("boom"); }, {}, "failing");
    JobHandle sibling = jobs.schedule([&siblingRan] { siblingRan.store(true); }, {}, "sibling");
    jobs.wait(failing);
    jobs.wait(sibling);

    CHECK(siblingRan.load());
}

TEST_CASE("recycled slots do not inherit stale poison")
{
    JobSystem jobs(4);

    // Fail, then reuse the pool heavily; every later job must still run.
    jobs.wait(jobs.schedule([] { throw std::runtime_error("boom"); }, {}, "failing"));

    std::atomic<int> counter{0};
    std::vector<JobHandle> handles;
    handles.reserve(2000);
    for (int i = 0; i < 2000; ++i)
        handles.push_back(jobs.schedule([&counter] { counter.fetch_add(1); }));
    jobs.waitAll(handles);

    CHECK(counter.load() == 2000);
}

// --- parallelFor chunk clamp (F5) ------------------------------------------
// A caller-supplied grain of 1 over a large count used to request one slot per
// element in a single burst, blowing past kMaxJobs.

TEST_CASE("parallelFor with grain 1 over a count far exceeding the pool completes")
{
    JobSystem jobs(4);
    constexpr size_t kCount = 100000;   // >> kMaxJobs (4096)

    std::vector<std::atomic<int>> visits(kCount);
    JobHandle handle = jobs.parallelFor(kCount, 1, [&visits](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i)
            visits[i].fetch_add(1, std::memory_order_relaxed);
    });
    jobs.wait(handle);

    for (size_t i = 0; i < kCount; ++i)
        REQUIRE(visits[i].load() == 1);   // every element covered exactly once
}

// --- Pool exhaustion is diagnosable, not UB (F5) ----------------------------
// allocateSlot used to assert-then-index, so under NDEBUG the check vanished
// and back()/pop_back() ran on an empty vector: silent pool corruption. It is
// now a logged abort in every build.
//
// Skipped by default because it deliberately kills the process. Run it on its
// own, in whichever build type you want to verify:
//   faye_tests --no-skip -tc="pool exhaustion aborts with a diagnostic"
// Expect exit 134 (SIGABRT) and a "Job pool exhausted" line on stderr.
TEST_CASE("pool exhaustion aborts with a diagnostic" * doctest::skip())
{
    JobSystem jobs(4);
    std::atomic<bool> gate{false};

    // Every job blocks, so no slot is ever recycled.
    std::vector<JobHandle> handles;
    for (uint32_t i = 0; i < JobSystem::kMaxJobs + 1000; ++i)
        handles.push_back(jobs.schedule([&gate] {
            while (!gate.load(std::memory_order_acquire))
                std::this_thread::yield();
        }));

    gate.store(true, std::memory_order_release);   // unreachable: abort happens above
    jobs.waitAll(handles);
}

TEST_CASE("a throwing parallelFor chunk still completes the parent, not a hang")
{
    JobSystem jobs(4);
    const uint64_t before = jobs.failureCount();

    // The chunk body reports into the parent counter AFTER the user code, so a
    // throw there used to leave the parent unfinished and wait() would hang.
    //
    // Trigger on the range COVERING an index, not on a chunk boundary: serial
    // mode collapses the whole span into one [0, count) call, so `begin == 500`
    // would never fire there and the test would silently pass for free.
    JobHandle handle = jobs.parallelFor(1000, 10, [](size_t begin, size_t end) {
        if (begin <= 500 && 500 < end)
            throw std::runtime_error("bad chunk");
    });
    jobs.wait(handle);   // must return

    CHECK(jobs.failureCount() > before);
}

TEST_CASE("a failed parallelFor poisons what depends on it")
{
    JobSystem jobs(4);
    std::atomic<bool> consumerRan{false};

    JobHandle produce = jobs.parallelFor(1000, 10, [](size_t begin, size_t) {
        if (begin == 0)
            throw std::runtime_error("bad chunk");
    });
    JobHandle consume = jobs.schedule([&consumerRan] { consumerRan.store(true); },
                                      {{produce}}, "consumer");
    jobs.wait(consume);

    CHECK_FALSE(consumerRan.load());
}

TEST_CASE("poison does not depend on link order: a dep that already failed still poisons")
{
    JobSystem jobs(4);
    std::atomic<bool> consumerRan{false};

    JobHandle producer = jobs.schedule([] { throw std::runtime_error("boom"); }, {}, "producer");
    jobs.wait(producer);   // fully complete AND recycled before we link anything

    JobHandle consumer = jobs.schedule([&consumerRan] { consumerRan.store(true); },
                                       {{producer}}, "consumer");
    jobs.wait(consumer);

    CHECK_FALSE(consumerRan.load());
}

TEST_CASE("a successful dep completed long ago does not poison")
{
    JobSystem jobs(4);
    std::atomic<bool> consumerRan{false};

    // Force a failure first so the record is non-empty (exercising the slow
    // path), then verify a clean dep is still treated as clean.
    jobs.wait(jobs.schedule([] { throw std::runtime_error("unrelated"); }, {}, "unrelated"));

    JobHandle producer = jobs.schedule([] {}, {}, "producer");
    jobs.wait(producer);

    JobHandle consumer = jobs.schedule([&consumerRan] { consumerRan.store(true); },
                                       {{producer}}, "consumer");
    jobs.wait(consumer);

    CHECK(consumerRan.load());
}
