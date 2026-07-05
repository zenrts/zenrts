#include <zenrts.h>
#include <iostream>

int main()
{
    std::cout << zenrts::greet("ZenRTS") << std::endl;
    std::cout << "Version: " << zenrts::version() << std::endl;
    return 0;
}
