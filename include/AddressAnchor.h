/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
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

#ifndef _AddressAnchor_h_
#define _AddressAnchor_h_

#include <Base.h>

extern int searchLinkBaseAddressExact(const void* arg1, const void* arg2);
extern int searchLinkBaseAddress(const void* arg1, const void* arg2);
extern int compareLinkBaseAddress(const void* arg1,const void* arg2);

class AddressAnchor {
private:
    Base* link;
    Base* linkedParent;

    uint32_t index;

    void dump8(BinaryOutputFile* b, uint32_t offset);
    void dump16(BinaryOutputFile* b, uint32_t offset);
    void dump32(BinaryOutputFile* b, uint32_t offset);
    void dump64(BinaryOutputFile* b, uint32_t offset);

    void dumpInstruction(BinaryOutputFile* b, uint32_t offset);
    void dumpDataReference(BinaryOutputFile* b, uint32_t offset);
public:
    // this gets accessed A LOT and is a performance bottleneck for instrumentation, so
    // we are basically caching link->getBaseAddress() and making it public so it doesn't
    // require a function access to get to it.
    uint64_t linkBaseAddress;

    AddressAnchor(Base* lnk, Base* par);
    ~AddressAnchor();

    uint64_t getLinkOffset();
    uint64_t getLinkValue();

    Base* getLink() { return link; }
    
    void refreshCache();
    Base* updateLink(Base* newLink);
    Base* getLinkedParent() { return linkedParent; }
    uint32_t getIndex() { return index; }

    void setIndex(uint32_t idx) { index = idx; }

    bool verify();
    void print();
    void dump(BinaryOutputFile* b, uint32_t offset);
};

#endif // _AddressAnchor_h_
