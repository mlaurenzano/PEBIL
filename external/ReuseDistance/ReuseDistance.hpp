/**
 * @file
 * @author Michael Laurenzano <michaell@sdsc.edu>
 * @version 0.01
 *
 * @section LICENSE
 * This file is part of the ReuseDistance tool.
 * 
 * Copyright (c) 2012, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * The ReuseDistanceHandler class allows for calculation and statistic tracking
 * for finding memory reuse distances given a stream of memory addresses and
 * ids.
 */

#include <assert.h>
#include <stdlib.h>
#include <tree234.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <list>
#include <map>
#include <vector>

// unordered_map is faster for many things, use it where sorted map isn't needed
#ifdef HAVE_UNORDERED_MAP
#include <unordered_map>
#define reuse_map_type std::unordered_map
#else
#define reuse_map_type std::map
#endif

#define TAB "\t"
#define ENDL "\n"

#define __seq id
int reusecmp (void* va, void* vb);

#define INFINITY_REUSE (0)
#define INVALID_SPATIAL (0xFFFFFFFFFFFFFFFFL)

/**
 * @struct ReuseEntry
 *
 * ReuseEntry is used to pass memory addresses into a ReuseDistance.
 *
 * @field id  The unique id of the entity which generated the memory address.
 * Statistics are tracked seperately for each unique id.
 * @field address  A memory address.
 */
struct ReuseEntry {
    uint64_t id;
    uint64_t address;
};


class ReuseStats;

/**
 * @class ReuseDistance
 *
 * Tracks reuse distances for a memory address stream. Keep track of the addresses within
 * a specific window of history, whose size can be finite or infinite. For basic usage, see
 * the documentation at http://bit.ly/ScqZVj for the constructors, the Process methods and
 * the Print methods. Also see the simple test file test/test.cpp included in this source 
 * package.
 */
class ReuseDistance {
private:
    // [sequence -> address] A counted B-tree filled with ReuseEntry*, sorted by __seq. this is from tree234.h
    tree234* window;

    // [address -> sequence]
    reuse_map_type<uint64_t, uint64_t> mwindow;

    uint64_t current;

protected:
    // store all stats
    // [id -> stats for this id]
    reuse_map_type<uint64_t, ReuseStats*> stats;

    uint64_t capacity;
    uint64_t sequence;
    uint64_t binindividual;
    uint64_t maxtracking;

    void Init(uint64_t w, uint64_t b);
    virtual ReuseStats* GetStats(uint64_t id, bool gen);
    virtual const std::string Describe() { return "REUSE"; }

public:

    static const uint64_t DefaultBinIndividual = 32;
    static const uint64_t Infinity = INFINITY_REUSE;

    /**
     * Contructs a ReuseDistance object.
     *
     * @param w  The maximum window size, or alternatively the maximum possible reuse distance that this tool
     * will find. No window/distance limit is imposed if ReuseDistance::Infinity is used, though you could easily
     * run of of memory.
     * @param b  All distances not greater than b will be tracked individually. All distances are tracked individually
     * if b == ReuseDistance::Infinity. Beyond individual tracking, distances are tracked in bins whose boundaries
     * are the powers of two greater than b (and not exeeding w, of course).
     *
     */
    ReuseDistance(uint64_t w, uint64_t b);

    /**
     * Contructs a ReuseDistance object. Equivalent to calling the other constructor with 
     * b == ReuseDistance::DefaultBinIndividual
     */
    ReuseDistance(uint64_t w);

    /**
     * Destroys a ReuseDistance object.
     */
    virtual ~ReuseDistance();

    /**
     * Print statistics for this ReuseDistance to an output stream.
     * The first line of the output is 7 tokens: [1] a string identifier
     * for the class (REUSESTATS or SPATIALSTATS), [2] the capacity or window 
     * size (0 == unlimited), [3] the maximum individual value being tracked, above
     * which values are tracked by bins whose boundaries are powers of 2,
     * [4] the maximum value to track, above which any value is considered
     * a miss. For ReuseDistance, this is equal to the capacity, for subclasses 
     * this can be different. [6] the number of ids that will be printed, 
     * [6] the total number of accesses
     * made (the number of ReuseEntry elements that were Process'ed) and
     * [7] the number of accesses that cold-misses or were outside the window range.
     * The stats for individual ids are printed on subsequent lines. The printing
     * of each id begins with a line which is comprised of 4 tokens: [1] a string
     * identifier (REUSEID or SPATIALID), [2] the id, [3] the number of accesses to that id and 
     * [4] the number of accesses for that id that were cold-misses or were 
     * outside the window range. Each subsequent line contains information about
     * a single bin for that id. These lines have 3 tokens: [1] and [2] the lower and
     * upper boundaries (both inclusive) of the bin and [3] the number of accesses
     * falling into that bin. See also ReuseDistance::PrintFormat
     *
     * @param f  The output stream to print results to.
     * @param annotate  Also print annotations describing the meaning of output fields, preceded by a '#'.
     *
     * @return none
     */
    virtual void Print(std::ostream& f, bool annotate=false);

    /**
     * Print statistics for this ReuseDistance to std::cout.
     * See the other version of ReuseDistance::Print for information about output format.
     *
     * @param annotate  Also print annotations describing the meaning of output fields, preceded by a '#'.
     *
     * @return none
     */
    virtual void Print(bool annotate=false);

    /**
     * Print information about the output format of ReuseDistance or one of its subclasses
     *
     * @param f  The stream to receive the output.
     *
     * @return none
     */
    void PrintFormat(std::ostream& f);

    /**
     * Process a single memory address.
     *
     * @param addr  The structure describing the memory address to process.
     *
     * @return none
     */
    virtual void Process(ReuseEntry& addr);

    /**
     * Process multiple memory addresses. Equivalent to calling Process on each element of the input array.
     *
     * @param addrs  An array of structures describing memory addresses to process.
     * @param count  The number of elements in addrs.
     *
     * @return none
     */
    void Process(ReuseEntry* addrs, uint64_t count);

    /**
     * Process multiple memory addresses. Equivalent to calling Process on each element of the input vector.
     *
     * @param addrs  A std::vector of memory addresses to process.
     *
     * @return none
     */
    void Process(std::vector<ReuseEntry> rs);

    /**
     * Process multiple memory addresses. Equivalent to calling Process on each element of the input vector.
     *
     * @param addrs  A std::vector of memory addresses to process.
     *
     * @return none
     */
    void Process(std::vector<ReuseEntry*> addrs);

    /**
     * Get the ReuseStats object associated with some unique id.
     *
     * @param id  The unique id.
     *
     * @return The ReuseStats object associated with parameter id, or NULL if no ReuseStats is associate with id.
     */
    ReuseStats* GetStats(uint64_t id);

    /**
     * Get a std::vector containing all of the unique indices processed
     * by this ReuseDistance object.
     *
     * @param ids  A std::vector which will contain the ids. It is an error to
     * pass this vector non-empty (that is addrs.size() == 0 is enforced at runtime).
     *
     * @return none
     */
    void GetIndices(std::vector<uint64_t>& ids);

    /**
     * Get a std::vector containing all of the addresses currently in this ReuseDistance
     * object's active window.
     *
     * @param addrs  A std::vector which will contain the addresses. It is an error to
     * pass this vector non-empty (that is addrs.size() == 0 is enforced at runtime).
     *
     * @return none
     */
    virtual void GetActiveAddresses(std::vector<uint64_t>& addrs);

    /**
     * Pretend that some number of addresses in the stream were skipped. Useful for intervel-based sampling.
     * This has the effect of flushing the entire window.
     * 
     * @param amount  The number of addresses to skip.
     *
     * @return none
     */
    virtual void SkipAddresses(uint64_t amount);
};

/**
 * @class ReuseStats
 *
 * ReuseStats holds count of observed reuse distances.
 */
class ReuseStats {
private:
    reuse_map_type<uint64_t, uint64_t> distcounts;
    uint64_t accesses;

    uint64_t id;
    uint64_t binindividual;
    uint64_t maxtracking;
    uint64_t invalid;

    uint64_t GetBin(uint64_t value);

public:

    /**
     * Contructs a ReuseStats object.
     *
     * @param idx  The unique id for this ReuseStats
     * @param bin  Stop collecting individual bins above this value
     * @param num  Any value above this is considered a miss
     * @param inv  The value which represents a miss
     */
    ReuseStats(uint64_t idx, uint64_t bin, uint64_t num, uint64_t inv)
        : accesses(0), id(idx), binindividual(bin), maxtracking(num), invalid(inv) {}

    /**
     * Destroys a ReuseStats object.
     */
    ~ReuseStats() {}

    /**
     * Increment the counter for some distance.
     *
     * @param dist  A reuse distance observed in the memory address stream.
     *
     * @return none
     */
    void Update(uint64_t dist);

    /**
     * Increment the number of misses. That is, addresses which were not found inside
     * the active address window. This is equivalent Update(0), but is faster.
     *
     * @return none
     */
    void Miss();

    /**
     * Get the number of misses. This is equal to the number of times
     * Update(ReuseDistance::Infinity) is called.
     *
     * @return The number of misses to this ReuseDistance object
     */
    virtual uint64_t GetMissCount();

    /**
     * Print a summary of the current reuse distances and counts for some id.
     *
     * @param f  The stream to receive the output.
     * @param annotate  Also print annotations describing the meaning of output fields, preceded by a '#'.
     *
     * @return none
     */
    virtual void Print(std::ostream& f, bool annotate=false);

    /**
     * Print information about the output format of ReuseStats
     *
     * @param f  The stream to receive the output.
     *
     * @return none
     */
    static void PrintFormat(std::ostream& f);

    /**
     * Get a std::vector containing the distances observed, sorted in ascending order.
     *
     * @param dists  The vector which will hold the sorted distance values. It is an error
     * for dists to be passed in non-empty (that is, dists.size() == 0 is enforced).
     *
     * @return none
     */
    void GetSortedDistances(std::vector<uint64_t>& dists);

    /**
     * Get the maximum distance observed.
     *
     * @return The maximum distance observed.
     */
    uint64_t GetMaximumDistance();

    /**
     * Count the number of times some distance has been observed.
     *
     * @param dist  The distance to count.
     *
     * @return The number of times d has been observed.
     */
    uint64_t CountDistance(uint64_t dist);

    /**
     * Count the total number of distances observed.
     *
     * @return The total number of distances observed.
     */
    uint64_t GetAccessCount();
};

/**
 * @class SpatialLocality
 *
 * Finds and tracks spatial locality within a memory address stream. Spatial locality is defined
 * as the minimum distance between the current address and any of the previous N addresses, as
 * in http://www.sdsc.edu/~allans/sc05_locality.pdf. This class allows that window size N to
 * be customized. For basic usage, see the documentation at http://bit.ly/ScqZVj for the 
 * constructors, the Process methods and the Print methods. Also see the simple test file 
 * test/test.cpp included in this source package.
 */
class SpatialLocality : public ReuseDistance {
private:

    // [address -> sequence]
    std::map<uint64_t, uint64_t> awindow;

    // list of the addresses in the window, ordered by sequence id
    std::list<uint64_t> swindow;


    void Init(uint64_t size, uint64_t bin, uint64_t max);

    virtual ReuseStats* GetStats(uint64_t id, bool gen);
    virtual const std::string Describe() { return "SPATIAL"; }

    static const uint64_t Invalid = INVALID_SPATIAL;

public:

    static const uint64_t DefaultWindowSize = 64;

    /**
     * Contructs a ReuseDistance object.
     *
     * @param w  The maximum window size, which is the maximum number of addresses that will be searched for spatial
     * locality. w != ReuseDistance::Infinity is enforced at runtime.
     * @param b  All distances not greater than b will be tracked individually. All distances are tracked individually
     * if b == ReuseDistance::Infinity. Beyond individual tracking, distances are tracked in bins whose boundaries
     * are the powers of two greater than b and not greater than n.
     * @param n  All distances greater than n will be counted as infinite. Use n == ReuseDistance::Infinity for no limit. n >= b is enforced at runtime.
     *
     */
    SpatialLocality(uint64_t w, uint64_t b, uint64_t n) : ReuseDistance(0) { SpatialLocality::Init(w, b, n); }

    /**
     * Constructs a SpatialLocality object. Equivalent to calling the other 3-argument constructor
     * with n == ReuseDistance::Infinity
     */
    SpatialLocality(uint64_t w, uint64_t b) : ReuseDistance(0) { SpatialLocality::Init(w, b, INFINITY_REUSE); }

    /**
     * Constructs a SpatialLocality object. Equivalent to calling the other 3-argument constructor
     * with w == b and n == ReuseDistance::Infinity
     */
    SpatialLocality(uint64_t w) : ReuseDistance(0) { SpatialLocality::Init(w, w, INFINITY_REUSE); }
 
    /**
     * Constructs a SpatialLocality object. Equivalent to calling the other 3-argument constructor
     * with w == b == SpatialLocality::DefaultWindowSize and n == ReuseDistance::Infinity
     */
    SpatialLocality() : ReuseDistance(0) {  SpatialLocality::Init(DefaultWindowSize, DefaultWindowSize, INFINITY_REUSE); }

    /**
     * Destroys a SpatialLocality object.
     */
    virtual ~SpatialLocality() {}

    /**
     * Get a std::vector containing all of the addresses currently in this SpatialLocality
     * object's active window.
     *
     * @param addrs  A std::vector which will contain the addresses. It is an error to
     * pass this vector non-empty (that is addrs.size() == 0 is enforced at runtime).
     *
     * @return none
     */
    virtual void GetActiveAddresses(std::vector<uint64_t>& addrs);

    /**
     * Process a single memory address.
     *
     * @param addr  The structure describing the memory address to process.
     *
     * @return none
     */
    virtual void Process(ReuseEntry& addr);

    /**
     * Pretend that some number of addresses in the stream were skipped. Useful for intervel-based sampling.
     * This has the effect of flushing the entire window.
     * 
     * @param amount  The number of addresses to skip.
     *
     * @return none
     */
    virtual void SkipAddresses(uint64_t amount);
};
