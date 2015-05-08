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

#include <iostream>
#include <string.h>
#include <stdio.h>

unsigned short classes_global = 5;

class Foo {
protected:
    int foo_field;
public:
    Foo(int i);
    virtual ~Foo() {}
    int getInt();
    virtual void print();
};

class Bar : public Foo {
protected:
    float bar_field;
public:
    Bar(float f);
    ~Bar() {}
    float getFloat();
    void print();
};

class Dum : public Foo {
protected:
    char* dum_field;
public:
    Dum(char* str);
    ~Dum() { delete[] dum_field; }
    char* getString();
    void print();
};

Foo::Foo(int i) {
    foo_field = i;
}
Bar::Bar(float f) : Foo((int)f) {
    bar_field = f;
}
Dum::Dum(char* str) : Foo(strlen(str)) {
    dum_field = str;
}
void Foo::print() {
    printf("FOO :: %d\n",getInt());
}
void Bar::print() {
    printf("BAR:: %f %d\n",getFloat(),getInt());
}
void Dum::print() {
    printf("FOO :: %s %d\n",dum_field,foo_field);
}
int Foo::getInt(){
    return foo_field;
}
float Bar::getFloat(){
    return bar_field;
}
char* Dum::getString(){
    return dum_field;
}

Foo cpp_global(25);

int main(){
    Foo* foo = new Foo(5);
    foo->print();
    delete foo;
    foo = new Bar(9.734);
    foo->print();
    printf("%d ",((Bar*)foo)->getInt());
    printf("%f ",((Bar*)foo)->getFloat());
    delete foo;
    foo = new Dum(strdup("What is going on here"));
    printf("%s\n",((Dum*)foo)->getString());
    foo->print();
    cpp_global.print();

    printf("%d\n",sizeof(char*));

    cpp_global = *foo;
    cpp_global.print();

    delete foo;
    printf("(Test Application Successfull)\n");

    return 0;
}

#include "foo.c"
#include "bar.c"
#include "dum.c"
