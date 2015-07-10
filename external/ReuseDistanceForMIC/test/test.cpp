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

#include <stdlib.h>
#include <ReuseDistance.hpp>

using namespace std;

#define debug(__stmt) __stmt

#define TINY_TEST (6)
#define SMALL_TEST (100)
#define MEDIUM_TEST (444444)
#define LARGE_TEST (3333333)
#define SEPERATOR "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"

int main(int argc, char* argv[]){

    uint32_t i, j;
    ReuseEntry entry = ReuseEntry();
    ReuseDistance* r1, *r2, *r3;
    ReuseDistance* s1, *s2, *s3;
    uint64_t filter = 0;
    if (argc > 1){
        filter = strtol(argv[1], NULL, 10);
    }

#define __test_define(__name, __size, __oiter, __inbegin, __initer, __ininc, ...) \
    r1 = new ReuseDistance(ReuseDistance::Infinity);\
    r2 = new ReuseDistance(__size * 2);\
    r3 = new ReuseDistance(__size / 2);\
    s1 = new SpatialLocality(1024, __size * 2, ReuseDistance::Infinity);\
    s2 = new SpatialLocality(64, 1, 32);\
    s3 = new SpatialLocality(128, __size / 2, ReuseDistance::Infinity);\
    entry.id = 0;\
    for (i = 0; i < __oiter; i++){\
        for (j = __inbegin; j < __initer; j += __ininc){\
            entry.address = j;\
            __VA_ARGS__;\
            r1->Process(entry);\
            r2->Process(entry);\
            r3->Process(entry);\
            s1->Process(entry);\
            s2->Process(entry);\
            s3->Process(entry);\
        }\
    }\
    cout << __name << " TEST" << ENDL;\
    cout << SEPERATOR;\
    r1->Print(true);\
    cout << SEPERATOR;\
    r2->Print();\
    cout << SEPERATOR;\
    r3->Print();\
    cout << SEPERATOR;\
    s1->Print(true);\
    cout << SEPERATOR;\
    s2->Print();\
    cout << SEPERATOR;\
    s3->Print();\
    cout << SEPERATOR;\
    cout << SEPERATOR;\
    delete r1;\
    delete r2;\
    delete r3;\
    delete s1;\
    delete s2;\
    delete s3;

    if (filter == 0 || filter == 1){
        __test_define("STRIDE-1", SMALL_TEST, 10, 0, SMALL_TEST, 1);
    }
    if (filter == 0 || filter == 2){
        __test_define("LRGRANGE", SMALL_TEST, 10, 0, SMALL_TEST*2, 1);
    }
    if (filter == 0 || filter == 3){
        __test_define("STRIDE-4", SMALL_TEST, 10, 0, SMALL_TEST, 4);
    }
    if (filter == 0 || filter == 4){
        __test_define("TRIANGLE", SMALL_TEST, SMALL_TEST, 0, i, 1);
    }
    if (filter == 0 || filter == 5){
        __test_define("IDDIFFER", SMALL_TEST, 10, 0, SMALL_TEST, 1, entry.id = j);
    }
    if (filter == 0 || filter == 6){
        __test_define("MEDTIMER", MEDIUM_TEST, 3, 0, MEDIUM_TEST, 1);
    }
    if (filter == 0 || filter == 7){
        __test_define("LRGTIMER", LARGE_TEST, 3, 0, LARGE_TEST, 1);
    }
    if (filter == 0 || filter == 8){
        __test_define("SHIFTRNG", TINY_TEST, TINY_TEST, (i % 2 == 0 ? (i) : (0)), (i % 2 == 0 ? (i+1) : (i*2)), 1);
    }

    return 0;
}


