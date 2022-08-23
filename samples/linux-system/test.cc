#include <set>
#include <iostream>

int main(int argc, char **argv)
{
    std::set<int> sss;
    sss.insert(1);
    sss.insert(2);
    sss.insert(2);
    sss.insert(3);

    for (auto v : sss) {
        std::cout << v << ", ";
    }
    std::cout << std::endl;

    return 0;
}
