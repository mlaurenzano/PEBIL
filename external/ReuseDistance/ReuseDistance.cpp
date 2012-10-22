/* 
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
 */

#include <ReuseDistance.hpp>

using namespace std;

#define REUSE_DEBUG
#ifdef REUSE_DEBUG
#define debug_assert(...) assert(__VA_ARGS__)
#else
#define debug_assert(...)
#endif

inline uint64_t uint64abs(uint64_t a){
    if (a < 0x8000000000000000L){
        return a;
    } else {
        return 0 - a;
    }
}

void ReuseDistance::Init(uint64_t w, uint64_t b){
    capacity = w;
    binindividual = b;
    maxtracking = capacity;

    current = 0;
    sequence = 1;

    window = newtree234();
    assert(window);

    mwindow.clear();

    assert(ReuseDistance::Infinity == NULL && "NULL is non-zero!?");
}

ReuseDistance::ReuseDistance(uint64_t w, uint64_t b){
    ReuseDistance::Init(w, b);
}

ReuseDistance::ReuseDistance(uint64_t w){
    ReuseDistance::Init(w, DefaultBinIndividual);
}

ReuseDistance::~ReuseDistance(){
    for (reuse_map_type<uint64_t, ReuseStats*>::const_iterator it = stats.begin(); it != stats.end(); it++){
        uint64_t id = it->first;
        delete stats[id];
    }

    debug_assert(current == count234(window));
    while (current){
        delete (ReuseEntry*)delpos234(window, 0);
        current--;
    }
    freetree234(window);
}

uint64_t ReuseStats::GetMissCount(){
    return distcounts[invalid];
}

void ReuseDistance::GetIndices(std::vector<uint64_t>& ids){
    assert(ids.size() == 0);
    for (reuse_map_type<uint64_t, ReuseStats*>::const_iterator it = stats.begin(); it != stats.end(); it++){
        uint64_t id = it->first;
        ids.push_back(id);
    }
}

void ReuseDistance::GetActiveAddresses(std::vector<uint64_t>& addrs){
    assert(addrs.size() == 0);
    debug_assert(current == count234(window));

    for (int i = 0; i < current; i++){
        ReuseEntry* r = index234(window, i);
        addrs.push_back(r->address);
    }
}

void ReuseDistance::Print(bool annotate){
    Print(cout, annotate);
}

void ReuseDistance::Process(ReuseEntry* rs, uint64_t count){
    for (uint32_t i = 0; i < count; i++){
        Process(rs[i]);
    }
}

void ReuseDistance::Process(vector<ReuseEntry> rs){
    for (vector<ReuseEntry>::const_iterator it = rs.begin(); it != rs.end(); it++){
        ReuseEntry r = *it;
        Process(r);
    }
}

void ReuseDistance::Process(vector<ReuseEntry*> rs){
    for (vector<ReuseEntry*>::const_iterator it = rs.begin(); it != rs.end(); it++){
        ReuseEntry* r = *it;
        Process((*r));
    }
}

void ReuseDistance::SkipAddresses(uint64_t amount){
    sequence += amount;

    // flush the window completely
    while (current){
        delete delpos234(window, 0);
        current--;
    }
    mwindow.clear();

    assert(mwindow.size() == 0);
    assert(count234(window) == 0);
}

void ReuseDistance::Process(ReuseEntry& r){
    uint64_t addr = r.address;
    uint64_t id = r.id;
    uint64_t mres = mwindow.count(addr);

    ReuseStats* stats = GetStats(id, true);

    int dist = 0;
    ReuseEntry* result;
    if (mres){
        mres = mwindow[addr];
        ReuseEntry key;
        key.address = addr;
        key.__seq = mres;

        result = findrelpos234(window, &key, &dist);
        debug_assert(result);

        if (capacity != ReuseDistance::Infinity){
            debug_assert(current - dist <= capacity);
        }
        stats->Update(current - dist);
    } else {
        stats->Update(ReuseDistance::Infinity);
    }

    // recycle a slot when possible
    ReuseEntry* slot = NULL;
    if (mres || (capacity != ReuseDistance::Infinity && current >= capacity)){
        slot = (ReuseEntry*)delpos234(window, dist);
        debug_assert(mwindow[slot->address]);
        mwindow.erase(slot->address);
        debug_assert(count234(window) == mwindow.size());
    } else {
        slot = new ReuseEntry();
        current++;
    }
    
    mwindow[addr] = sequence;

    slot->__seq = sequence;
    slot->address = addr;
    add234(window, slot);

    debug_assert(count234(window) == mwindow.size());
    debug_assert(mwindow.size() <= current);

    sequence++;
}

void ReuseDistance::PrintFormat(ostream& f){
    f << "# "
      << Describe() << "STATS"
      << TAB << "<window_size>"
      << TAB << "<bin_indiv>"
      << TAB << "<max_track>"
      << TAB << "<id_count>"
      << TAB << "<tot_access>"
      << TAB << "<tot_miss>"
      << ENDL;

    f << "# "
      << TAB << Describe() << "ID"
      << TAB << "<id>"
      << TAB << "<id_access>"
      << TAB << "<id_miss>"
      << ENDL;
}

void ReuseStats::PrintFormat(ostream& f){
    f << "# "
      << TAB 
      << TAB << "<bin_lower_bound>"
      << TAB << "<bin_upper_bound>"
      << TAB << "<bin_count>"
      << ENDL;
}

void ReuseDistance::Print(ostream& f, bool annotate){
    vector<uint64_t> keys;
    for (reuse_map_type<uint64_t, ReuseStats*>::const_iterator it = stats.begin(); it != stats.end(); it++){
        keys.push_back(it->first);
    }
    sort(keys.begin(), keys.end());

    uint64_t tot = 0, mis = 0;
    for (vector<uint64_t>::const_iterator it = keys.begin(); it != keys.end(); it++){
        uint64_t id = (*it);
        ReuseStats* r = (ReuseStats*)stats[id];
        tot += r->GetAccessCount();
        mis += r->GetMissCount();
    }

    if (annotate){
        ReuseDistance::PrintFormat(f);
        ReuseStats::PrintFormat(f);
    }

    f << Describe() << "STATS"
      << TAB << dec << capacity
      << TAB << binindividual
      << TAB << maxtracking
      << TAB << keys.size()
      << TAB << tot
      << TAB << mis
      << ENDL;

    for (vector<uint64_t>::const_iterator it = keys.begin(); it != keys.end(); it++){
        uint64_t id = (*it);
        ReuseStats* r = (ReuseStats*)stats[id];

        f << TAB << Describe() << "ID"
          << TAB << dec << id
          << TAB << r->GetAccessCount()
          << TAB << r->GetMissCount()
          << ENDL;

        r->Print(f);
    }
}

ReuseStats* ReuseDistance::GetStats(uint64_t id, bool gen){
    ReuseStats* s = stats[id];
    if (s == NULL && gen){
        s = new ReuseStats(id, binindividual, capacity, ReuseDistance::Infinity);
        stats[id] = s;
    }
    return s;
}

// this should be fast as possible. This code is from http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
static const uint64_t b[] = {0x2L, 0xCL, 0xF0L, 0xFF00L, 0xFFFF0000L, 0xFFFFFFFF00000000L};
static const uint32_t S[] = {1, 2, 4, 8, 16, 32};
inline uint64_t ShaveBitsPwr2(uint64_t val){
    val -= 1;
    register uint64_t r = 0; // result of log2(v) will go here
    for (int32_t i = 5; i >= 0; i--){
        if (val & b[i]){
            val = val >> S[i];
            r |= S[i];
        }
    }
    return (2 << r);
}

uint64_t ReuseStats::GetBin(uint64_t value){
    // not a valid value
    if (value == invalid){
        return invalid;
    }
    // outside of tracking window, also invalid
    else if (maxtracking != ReuseDistance::Infinity && value > maxtracking){
        return invalid;
    }
    // valid but not tracked individually
    else if (binindividual != ReuseDistance::Infinity && value > binindividual){
        return ShaveBitsPwr2(value);
    }
    // valid and tracked individually
    return value;
}

ReuseStats* ReuseDistance::GetStats(uint64_t id){
    return GetStats(id, false);
}

uint64_t ReuseStats::GetAccessCount(){
    return accesses;
}

uint64_t ReuseStats::GetMaximumDistance(){
    uint64_t max = 0;
    for (reuse_map_type<uint64_t, uint64_t>::const_iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        if (d > max){
            max = d;
        }
    }
    return max;
}

void ReuseStats::Update(uint64_t dist){
    distcounts[GetBin(dist)] += 1;
    accesses++;
}

uint64_t ReuseStats::CountDistance(uint64_t d){
    if (distcounts.count(d) == 0){
        return 0;
    }
    return distcounts[d];
}

void ReuseStats::GetSortedDistances(vector<uint64_t>& dkeys){
    assert(dkeys.size() == 0 && "dkeys must be an empty vector");
    for (reuse_map_type<uint64_t, uint64_t>::const_iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        dkeys.push_back(d);
    }
    sort(dkeys.begin(), dkeys.end());    
}

void ReuseStats::Print(ostream& f, bool annotate){
    vector<uint64_t> keys;
    GetSortedDistances(keys);

    if (annotate){
        ReuseStats::PrintFormat(f);
    }

    for (vector<uint64_t>::const_iterator it = keys.begin(); it != keys.end(); it++){
        uint64_t d = *it;
        if (d == invalid) continue;

        debug_assert(distcounts.count(d) > 0);
        uint32_t cnt = distcounts[d];

        debug_assert(cnt > 0);
        if (cnt > 0){
            uint64_t p = d / 2 + 1;
            if (binindividual == ReuseDistance::Infinity || d <= binindividual){
                p = d;
            }
            f << TAB
              << TAB << dec << p
              << TAB << d
              << TAB << cnt
              << ENDL;
        }
    }
}

void SpatialLocality::Init(uint64_t size, uint64_t bin, uint64_t max){
    sequence = 1;
    capacity = size;
    binindividual = bin;
    maxtracking = max;

    assert(capacity > 0 && capacity != ReuseDistance::Infinity && "window size must be a finite, positive value");
    assert((maxtracking == INFINITY_REUSE || maxtracking >= binindividual) && "max tracking must be at least as large as individual binning");
}

ReuseStats* SpatialLocality::GetStats(uint64_t id, bool gen){
    ReuseStats* s = stats[id];
    if (s == NULL && gen){
        s = new ReuseStats(id, binindividual, maxtracking, SpatialLocality::Invalid);
        stats[id] = s;
    }
    return s;
}

void SpatialLocality::Process(ReuseEntry& r){
    uint64_t addr = r.address;
    uint64_t id = r.id;
    ReuseStats* stats = GetStats(id, true);
    debug_assert(stats);

    // find the address closest to addr
    uint64_t bestdiff = SpatialLocality::Invalid;

    if (awindow.size() > 0){

        map<uint64_t, uint64_t>::const_iterator it = awindow.upper_bound(addr);
        if (it == awindow.end()){
            it--;
        }

        // only need to check the values immediately equal, >, and < than addr
        for (uint32_t i = 0; i < 3; i++, it--){
            uint64_t cur = it->first;
            uint64_t seq = it->second;
            uint64_t diff = uint64abs(cur - addr);

            if (diff < bestdiff){
                bestdiff = diff;
            }

            if (it == awindow.begin()){
                break;
            }
        }
    }

    stats->Update(bestdiff);

    // remove the oldest address in the window
    if (swindow.size() > capacity){
        uint64_t a = swindow.front();
        swindow.pop_front();

        uint64_t v = awindow[a];
        if (v > 1){
            awindow[a] = v - 1;
        } else {
            awindow.erase(a);
        }
    }

    // insert the newest address into the window
    awindow[addr]++;
    swindow.push_back(addr);
}

void SpatialLocality::SkipAddresses(uint64_t amount){

    // flush the window completely
    while (swindow.size()){
        uint64_t a = swindow.front();
        swindow.pop_front();

        uint64_t v = awindow[a];
        if (v > 1){
            awindow[a] = v - 1;
        } else {
            awindow.erase(a);
        }
    }

    assert(awindow.size() == 0);
    assert(swindow.size() == 0);
}

void SpatialLocality::GetActiveAddresses(std::vector<uint64_t>& addrs){
    assert(addrs.size() == 0);

    for (map<uint64_t, uint64_t>::const_iterator it = awindow.begin(); it != awindow.end(); it++){
        uint64_t addr = it->first;
        addrs.push_back(addr);
    }
}

