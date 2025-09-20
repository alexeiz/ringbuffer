#include "ringbuffer/ringbuffer.hpp"
#include <iostream>
#include <stdexcept>

using namespace std;
using namespace rb;

int main(int argc, char * argv[])
try
{
    if (argc < 2)
        return 0;

    ring_buffer_reader<int> rb{argv[1]};
    int pos = 0;
    for (int i : rb)
        cout << pos++ << '\t' << i << endl;
}
catch (exception & e)
{
    cerr << "error: " << e.what() << endl;
}
