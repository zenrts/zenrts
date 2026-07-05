#include <zenrts/utils.h>

namespace zenrts {

std::string greet(std::string_view name)
{
    return "Hello, " + std::string(name) + "!";
}

}
