#include <gui/model/FrequencyParameters.hpp>
#include <assert.h>
#include <stdio.h>
static void input(FrequencyParameters& p, const char* s)
{
    p.clear();
    while (*s) assert(p.digit(*s++ - '0'));
}
int main()
{
    FrequencyParameters p;
    for (unsigned i=0; i<3; ++i) assert(p.get(i)==1000);
    assert(p.digit(2));
    assert(strcmp(p.draft(), "2")==0 && p.get(0)==1000);
    assert(!p.commit());
    input(p,"100"); assert(p.commit() && p.get(0)==100);
    p.select(1); input(p,"10000"); assert(p.commit() && p.get(1)==10000);
    p.select(2); input(p,"2500"); assert(p.commit() && p.get(2)==2500);
    assert(p.get(0)==100 && p.get(1)==10000);
    input(p,"99"); assert(!p.commit() && p.get(2)==2500);
    input(p,"10001"); assert(!p.commit() && p.get(2)==2500);
    p.clear(); assert(!p.commit()); p.backspace(); assert(!p.commit());
    input(p,"99999"); assert(!p.digit(9)); assert(!p.commit());
    p.cancel(); assert(strcmp(p.draft(),"2500")==0);
    p.backspace(); assert(strcmp(p.draft(),"250")==0); assert(p.commit());
    input(p,"888"); p.select(0); p.select(2); assert(strcmp(p.draft(),"250")==0);
    assert(!p.select(3) && p.selected()==2 && p.get(3)==0);
    p.clear(); p.digit(0); p.digit(0); p.digit(5); assert(strcmp(p.draft(),"5")==0);
    assert(!p.digit(10));
    puts("PASS: independent values, boundaries, invalid input, cancel, backspace, channel switch");
}
