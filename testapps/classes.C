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
