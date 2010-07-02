/* This program is free software: you can redistribute it and/or modify
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

#ifndef _CallReplace_h_
#define _CallReplace_h_

#include <InstrumentationTool.h>

class Symbol;

class CallReplace : public InstrumentationTool {
private:
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;
    Vector<InstrumentationFunction*> functionWrappers;

    Vector<char*>* functionList;
    Vector<char*> libraries;
public:
    CallReplace(ElfFile* elf, char* traceFile, char* libList, char* ext, bool lpi, bool dtl);
    ~CallReplace();

    void declare();
    void instrument();

    char* getWrapperName(uint32_t idx);
    char* getFunctionName(uint32_t idx);
};


#endif /* _CallReplace_h_ */
